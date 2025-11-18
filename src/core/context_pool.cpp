// src/core/context_pool.cpp
#include "core/context_pool.h"
#include "spdlog/spdlog.h"
#include <stdexcept>

// ==============================================================================
//  ContextGuard Sınıfı Implementasyonu
// ==============================================================================

ContextGuard::ContextGuard(LlamaContextPool* pool, llama_context* ctx, int id)
    : pool_(pool), ctx_(ctx), id_(id) {
    if (!ctx_) {
        throw std::runtime_error("Acquired null llama_context from pool.");
    }
    spdlog::debug("[Pool] ContextGuard created for ctx_id={}", id_);
}

ContextGuard::~ContextGuard() {
    if (ctx_ && pool_) {
        spdlog::debug("[Pool] ContextGuard destroyed for ctx_id={}. Releasing context.", id_);
        
        // --- DÜZELTME: Proje dokümantasyonunda belirtilen TEK DOĞRU yöntemi uygula ---
        spdlog::debug("[Pool] Clearing KV cache for ctx_id={} before release using llama_memory_seq_rm...", id_);
        // Bütün dizinleri (-1) ve bütün pozisyonları (-1, -1) temizle.
        llama_memory_seq_rm(llama_get_memory(ctx_), -1, -1, -1);
        spdlog::debug("[Pool] KV cache cleared for ctx_id={}.", id_);
        
        pool_->release(ctx_, id_);
    }
}

// Taşıma Kurucusu (Move Constructor)
ContextGuard::ContextGuard(ContextGuard&& other) noexcept
    : pool_(other.pool_), ctx_(other.ctx_), id_(other.id_) {
    spdlog::debug("[Pool] ContextGuard move constructor called for ctx_id={}", id_);
    other.pool_ = nullptr;
    other.ctx_ = nullptr;
    other.id_ = -1;
}

// Taşıma Atama Operatörü (Move Assignment Operator)
ContextGuard& ContextGuard::operator=(ContextGuard&& other) noexcept {
    spdlog::debug("[Pool] ContextGuard move assignment called for ctx_id={}", id_);
    if (this != &other) {
        if (ctx_ && pool_) {
            // Önceki kaynağı düzgünce serbest bırak
            llama_memory_seq_rm(llama_get_memory(ctx_), -1, -1, -1);
            pool_->release(ctx_, id_);
        }
        pool_ = other.pool_;
        ctx_ = other.ctx_;
        id_ = other.id_;
        other.pool_ = nullptr;
        other.ctx_ = nullptr;
        other.id_ = -1;
    }
    return *this;
}

// ==============================================================================
//  LlamaContextPool Sınıfı Implementasyonu
// ==============================================================================

LlamaContextPool::LlamaContextPool(const Settings& settings, llama_model* model, prometheus::Gauge& active_contexts_gauge)
    : model_(model), settings_(settings), active_contexts_gauge_(active_contexts_gauge) {
    
    max_size_ = settings.n_threads;
    if (max_size_ == 0) {
        max_size_ = 1;
    }

    spdlog::info("Context Pool: Initializing {} llama_context instances...", max_size_);
    initialize_contexts();
    
    active_contexts_gauge_.Set(0);
}

LlamaContextPool::~LlamaContextPool() {
    std::lock_guard<std::mutex> lock(mutex_);
    while (!pool_.empty()) {
        auto [ctx, id] = pool_.front();
        pool_.pop();
        llama_free(ctx);
        spdlog::debug("[Pool] Destroyed ctx_id={}", id);
    }
    spdlog::info("Context Pool: All instances destroyed.");
}

void LlamaContextPool::initialize_contexts() {
    if (!model_) {
        return;
    }

    for (size_t i = 0; i < max_size_; ++i) {
        llama_context_params ctx_params = llama_context_default_params();
        ctx_params.n_ctx           = settings_.context_size;
        ctx_params.n_threads       = settings_.n_threads;
        ctx_params.n_threads_batch = settings_.n_threads_batch;
        ctx_params.offload_kqv     = settings_.kv_offload;

        llama_context* ctx = llama_init_from_model(model_, ctx_params);
        if (!ctx) {
            throw std::runtime_error("Failed to create llama_context for pool instance.");
        }
        int id = next_id_++;
        pool_.push({ctx, id});
        spdlog::debug("[Pool] Initialized and added ctx_id={}", id);
    }
}

ContextGuard LlamaContextPool::acquire() {
    std::unique_lock<std::mutex> lock(mutex_);
    
    spdlog::debug("[Pool] Thread waiting to acquire context... (Pool size: {})", pool_.size());
    cv_.wait(lock, [this]{ return !pool_.empty(); });
    
    auto [ctx, id] = pool_.front();
    pool_.pop();
    
    active_contexts_gauge_.Set(get_active_count());
    spdlog::info("[Pool] Acquired ctx_id={}. Active contexts: {}.", id, get_active_count());
    
    return ContextGuard(this, ctx, id);
}

void LlamaContextPool::release(llama_context* ctx, int id) {
    if (!ctx) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        pool_.push({ctx, id});
    }
    
    active_contexts_gauge_.Set(get_active_count());
    spdlog::info("[Pool] Released ctx_id={}. Active contexts: {}.", id, get_active_count());
    
    cv_.notify_one();
}
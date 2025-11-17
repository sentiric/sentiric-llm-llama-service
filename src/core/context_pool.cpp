#include "core/context_pool.h"
#include "spdlog/spdlog.h"
#include <stdexcept>

// ==============================================================================
//  ContextGuard Sınıfı Implementasyonu
// ==============================================================================

ContextGuard::ContextGuard(LlamaContextPool* pool, llama_context* ctx)
    : pool_(pool), ctx_(ctx) {
    if (!ctx_) {
        throw std::runtime_error("Acquired null llama_context from pool.");
    }
}

ContextGuard::~ContextGuard() {
    if (ctx_ && pool_) {
        // ÖNEMLİ: Context'i havuza geri bırakmadan önce KV cache'ini temizle.
        llama_memory_seq_rm(llama_get_memory(ctx_), -1, -1, -1);
        pool_->release(ctx_);
    }
}

// Taşıma Kurucusu (Move Constructor)
ContextGuard::ContextGuard(ContextGuard&& other) noexcept
    : pool_(other.pool_), ctx_(other.ctx_) {
    other.pool_ = nullptr;
    other.ctx_ = nullptr;
}

// Taşıma Atama Operatörü (Move Assignment Operator)
ContextGuard& ContextGuard::operator=(ContextGuard&& other) noexcept {
    if (this != &other) {
        if (ctx_ && pool_) {
            llama_memory_seq_rm(llama_get_memory(ctx_), -1, -1, -1);
            pool_->release(ctx_);
        }
        pool_ = other.pool_;
        ctx_ = other.ctx_;
        other.pool_ = nullptr;
        other.ctx_ = nullptr;
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
        max_size_ = 1; // En az bir thread olmalı
    }

    spdlog::info("Context Pool: Initializing {} llama_context instances...", max_size_);
    initialize_contexts();
    
    // Başlangıçta aktif context sayısını 0 olarak ayarla
    active_contexts_gauge_.Set(0);
}

LlamaContextPool::~LlamaContextPool() {
    std::lock_guard<std::mutex> lock(mutex_);
    while (!pool_.empty()) {
        llama_context* ctx = pool_.front();
        pool_.pop();
        llama_free(ctx);
    }
    spdlog::info("Context Pool: Destroyed {} llama_context instances.", max_size_);
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
        pool_.push(ctx);
    }
}

ContextGuard LlamaContextPool::acquire() {
    std::unique_lock<std::mutex> lock(mutex_);
    
    // Havuzda boş context kalmayıncaya kadar bekle.
    cv_.wait(lock, [this]{ return !pool_.empty(); });
    
    llama_context* ctx = pool_.front();
    pool_.pop();
    
    // Aktif context sayısını güncelle
    active_contexts_gauge_.Set(get_active_count());
    
    return ContextGuard(this, ctx);
}

void LlamaContextPool::release(llama_context* ctx) {
    if (!ctx) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        pool_.push(ctx);
    }
    
    // Aktif context sayısını güncelle
    active_contexts_gauge_.Set(get_active_count());
    
    // Havuza yeni bir context eklendiğini bekleyen bir thread'e haber ver.
    cv_.notify_one();
}
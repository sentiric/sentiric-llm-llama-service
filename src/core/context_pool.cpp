// src/core/context_pool.cpp
#include "core/context_pool.h"
#include "spdlog/spdlog.h"
#include <stdexcept>
#include <algorithm>

// --- ContextGuard ---
ContextGuard::ContextGuard(LlamaContextPool* pool, llama_context* ctx, int id, size_t matched_tokens)
    : pool_(pool), ctx_(ctx), id_(id), matched_tokens_(matched_tokens) {}

ContextGuard::~ContextGuard() {
    if (ctx_ && pool_) {
        pool_->release(ctx_, id_, {}); 
    }
}

void ContextGuard::release_early(const std::vector<llama_token>& final_tokens) {
    if (ctx_ && pool_) {
        pool_->release(ctx_, id_, final_tokens);
        ctx_ = nullptr;
        pool_ = nullptr;
    }
}

ContextGuard::ContextGuard(ContextGuard&& other) noexcept
    : pool_(other.pool_), ctx_(other.ctx_), id_(other.id_), matched_tokens_(other.matched_tokens_) {
    other.pool_ = nullptr; other.ctx_ = nullptr; other.id_ = -1;
}

ContextGuard& ContextGuard::operator=(ContextGuard&& other) noexcept {
    if (this != &other) {
        if (ctx_ && pool_) pool_->release(ctx_, id_, {});
        pool_ = other.pool_; ctx_ = other.ctx_; id_ = other.id_; matched_tokens_ = other.matched_tokens_;
        other.pool_ = nullptr; other.ctx_ = nullptr; other.id_ = -1;
    }
    return *this;
}

// --- LlamaContextPool ---

LlamaContextPool::LlamaContextPool(const Settings& settings, llama_model* model, prometheus::Gauge& active_contexts_gauge)
    : model_(model), settings_(settings), active_contexts_gauge_(active_contexts_gauge) {
    
    // --- MİMARİ DÜZELTME: Hız vs Kapasite Ayrımı ---
    // Eskiden: max_size_ = settings.n_threads; (Yanlış: 6 thread = 6 context)
    // Yeni: Eğer batching kapalıysa (Yerel Mod) -> Sadece 1 Context.
    //       Eğer batching açıksa -> Max Batch Size kadar Context.
    if (settings.enable_dynamic_batching) {
        max_size_ = settings.max_batch_size;
    } else {
        max_size_ = 1; // Yerel CPU/GPU kullanımı için en performanslısı budur.
    }
    
    if (max_size_ == 0) max_size_ = 1;
    
    contexts_.resize(max_size_);
    is_busy_.assign(max_size_, false);

    spdlog::info("Context Pool: Initializing {} SMART contexts (Threads per ctx: {})...", max_size_, settings.n_threads);
    initialize_contexts();
    active_contexts_gauge_.Set(0);
}

LlamaContextPool::~LlamaContextPool() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& state : contexts_) {
        if (state.ctx) llama_free(state.ctx);
    }
}

void LlamaContextPool::initialize_contexts() {
    if (!model_) return;
    for (size_t i = 0; i < max_size_; ++i) {
        llama_context_params ctx_params = llama_context_default_params();
        
        // --- OPTİMİZASYON: CPU İçin Batch Size Sınırı ---
        // Büyük promt'larda arayüzün donmaması için batch'i 512'de tutuyoruz.
        // Bu, CPU'nun nefes almasını sağlar.
        ctx_params.n_ctx = settings_.context_size;
        ctx_params.n_batch = std::min((uint32_t)settings_.context_size, (uint32_t)512);
        
        // İşlem gücü (Compute Power) buradan gelir
        ctx_params.n_threads = settings_.n_threads;
        ctx_params.n_threads_batch = settings_.n_threads_batch;
        
        ctx_params.offload_kqv = settings_.kv_offload;
        
        llama_context* ctx = llama_init_from_model(model_, ctx_params);
        if (!ctx) throw std::runtime_error("Failed to create llama_context.");

        contexts_[i] = {ctx, (int)i, {}, std::chrono::steady_clock::now()};
    }
}

ContextGuard LlamaContextPool::acquire() {
    return acquire({});
}

ContextGuard LlamaContextPool::acquire(const std::vector<llama_token>& input_tokens) {
    std::unique_lock<std::mutex> lock(mutex_);
    
    cv_.wait(lock, [this]{ 
        return std::any_of(is_busy_.begin(), is_busy_.end(), [](bool busy){ return !busy; }); 
    });

    int best_id = -1;
    size_t max_match = 0;
    
    for (size_t i = 0; i < max_size_; ++i) {
        if (is_busy_[i]) continue;

        const auto& cached_tokens = contexts_[i].tokens;
        size_t match = 0;
        size_t limit = std::min(input_tokens.size(), cached_tokens.size());
        
        while (match < limit && input_tokens[match] == cached_tokens[match]) {
            match++;
        }

        if (best_id == -1 || match > max_match) {
            max_match = match;
            best_id = i;
        }
    }

    if (best_id == -1) {
         for (size_t i = 0; i < max_size_; ++i) {
            if (!is_busy_[i]) { best_id = i; break; }
         }
    }
    
    if (best_id == -1) throw std::runtime_error("Unexpected pool state: no context available.");

    is_busy_[best_id] = true;
    active_contexts_gauge_.Set(get_active_count());
    
    if (max_match > 0) {
        spdlog::info("⚡ SMART CACHE HIT! Context #{} reused with {} matching tokens.", best_id, max_match);
    }

    return ContextGuard(this, contexts_[best_id].ctx, best_id, max_match);
}

void LlamaContextPool::release(llama_context* ctx, int id, const std::vector<llama_token>& current_tokens) {
    if (!ctx) return;
    
    std::lock_guard<std::mutex> lock(mutex_);
    if (id >= 0 && id < (int)contexts_.size()) {
        contexts_[id].tokens = current_tokens;
        contexts_[id].last_used = std::chrono::steady_clock::now();
        is_busy_[id] = false;
    }
    
    active_contexts_gauge_.Set(get_active_count());
    cv_.notify_one();
}

size_t LlamaContextPool::get_active_count() const {
    return std::count(is_busy_.begin(), is_busy_.end(), true);
}
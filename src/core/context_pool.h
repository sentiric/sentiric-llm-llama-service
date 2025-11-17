#pragma once

#include "config.h"
#include "llama.h"
#include <memory>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <prometheus/gauge.h>

class LlamaContextPool;

class ContextGuard {
public:
    ContextGuard(LlamaContextPool* pool, llama_context* ctx);
    ~ContextGuard();
    ContextGuard(const ContextGuard&) = delete;
    ContextGuard& operator=(const ContextGuard&) = delete;
    ContextGuard(ContextGuard&& other) noexcept;
    ContextGuard& operator=(ContextGuard&& other) noexcept;

    llama_context* get() { return ctx_; }

private:
    LlamaContextPool* pool_;
    llama_context* ctx_;
};

class LlamaContextPool {
public:
    LlamaContextPool(const Settings& settings, llama_model* model, prometheus::Gauge& active_contexts_gauge);
    ~LlamaContextPool();
    ContextGuard acquire();
    void release(llama_context* ctx);
    
    size_t get_active_count() const { return max_size_ - pool_.size(); }
    size_t get_total_count() const { return max_size_; }
    llama_model* get_model() const { return model_; } // YENİ: Model erişimi

private:
    void initialize_contexts();
    llama_model* model_;
    const Settings& settings_;
    size_t max_size_;
    std::queue<llama_context*> pool_;
    std::mutex mutex_;
    std::condition_variable cv_;
    prometheus::Gauge& active_contexts_gauge_;
};
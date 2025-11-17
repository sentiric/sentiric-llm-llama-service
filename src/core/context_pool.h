#pragma once

#include "config.h"
#include "llama.h"
#include <memory>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <prometheus/gauge.h>

class LlamaContextPool;

// RAII prensibiyle context'in güvenli kullanımını sağlar.
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

// Eş zamanlı istekler için bir 'llama_context' havuzu yönetir.
class LlamaContextPool {
public:
    LlamaContextPool(const Settings& settings, llama_model* model, prometheus::Gauge& active_contexts_gauge);
    ~LlamaContextPool();
    ContextGuard acquire();
    void release(llama_context* ctx);
    
    size_t get_active_count() const { return max_size_ - pool_.size(); }

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
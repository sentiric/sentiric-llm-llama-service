// src/core/context_pool.h
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
    ContextGuard(LlamaContextPool* pool, llama_context* ctx, int id); // ID eklendi
    ~ContextGuard();
    ContextGuard(const ContextGuard&) = delete;
    ContextGuard& operator=(const ContextGuard&) = delete;
    ContextGuard(ContextGuard&& other) noexcept;
    ContextGuard& operator=(ContextGuard&& other) noexcept;

    llama_context* get() { return ctx_; }
    int get_id() const { return id_; } // ID'yi almak için getter

private:
    LlamaContextPool* pool_;
    llama_context* ctx_;
    int id_; // Context'i izlemek için ID
};

class LlamaContextPool {
public:
    LlamaContextPool(const Settings& settings, llama_model* model, prometheus::Gauge& active_contexts_gauge);
    ~LlamaContextPool();
    ContextGuard acquire();
    void release(llama_context* ctx, int id); // ID eklendi
    
    size_t get_active_count() const { return max_size_ - pool_.size(); }
    size_t get_total_count() const { return max_size_; }
    llama_model* get_model() const { return model_; }

private:
    void initialize_contexts();
    llama_model* model_;
    const Settings& settings_;
    size_t max_size_;
    // Havuzu context ve ID'si ile birlikte tutmak için std::pair kullanıyoruz
    std::queue<std::pair<llama_context*, int>> pool_; 
    std::mutex mutex_;
    std::condition_variable cv_;
    prometheus::Gauge& active_contexts_gauge_;
    int next_id_ = 0; // ID atamak için sayaç
};
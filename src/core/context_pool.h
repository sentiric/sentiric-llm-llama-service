// src/core/context_pool.h
#pragma once

#include "config.h"
#include "llama.h"
#include <memory>
#include <mutex>
#include <vector>
#include <deque>
#include <condition_variable>
#include <prometheus/gauge.h>
#include <map>
#include <chrono> 

class LlamaContextPool;

class ContextGuard {
public:
    ContextGuard(LlamaContextPool* pool, llama_context* ctx, int id, size_t matched_tokens);
    ~ContextGuard();
    
    // Move semantics
    ContextGuard(const ContextGuard&) = delete;
    ContextGuard& operator=(const ContextGuard&) = delete;
    ContextGuard(ContextGuard&& other) noexcept;
    ContextGuard& operator=(ContextGuard&& other) noexcept;

    llama_context* get() { return ctx_; }
    int get_id() const { return id_; }
    size_t get_matched_tokens() const { return matched_tokens_; }

    // --- YENİ EKLE ---
    // Context'i güvenli bir şekilde erkenden havuza bırakır
    void release_early(const std::vector<llama_token>& final_tokens);

private:
    LlamaContextPool* pool_;
    llama_context* ctx_;
    int id_;
    size_t matched_tokens_;
};

class LlamaContextPool {
public:
    LlamaContextPool(const Settings& settings, llama_model* model, prometheus::Gauge& active_contexts_gauge);
    ~LlamaContextPool();

    // --- DÜZELTME BURADA: ÇİFT İMZA (OVERLOADING) ---
    
    // 1. Akıllı Cache (LLMEngine kullanır - Parametreli)
    ContextGuard acquire(const std::vector<llama_token>& input_tokens);
    
    // 2. Legacy/Warmup (ModelWarmup kullanır - Parametresiz)
    // Bu fonksiyon, mevcut kodun patlamamasını sağlayacak.
    ContextGuard acquire(); 
    
    void release(llama_context* ctx, int id, const std::vector<llama_token>& current_tokens); 
    
    size_t get_active_count() const;
    size_t get_total_count() const { return max_size_; }
    llama_model* get_model() const { return model_; }

private:
    struct ContextState {
        llama_context* ctx;
        int id;
        std::vector<llama_token> tokens;
        std::chrono::steady_clock::time_point last_used;
    };

    void initialize_contexts();
    
    llama_model* model_;
    const Settings& settings_;
    size_t max_size_;
    
    std::vector<ContextState> contexts_;
    std::vector<bool> is_busy_;

    std::mutex mutex_;
    std::condition_variable cv_;
    prometheus::Gauge& active_contexts_gauge_;
};
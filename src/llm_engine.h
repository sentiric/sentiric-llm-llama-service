#pragma once

#include "config.h"
#include "llama.h"
#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <memory>
#include <mutex>
#include <queue>
#include <condition_variable>
#include "sentiric/llm/v1/local.pb.h"
#include <stdexcept>

class LlamaContextPool;

class ContextGuard {
public:
    ContextGuard(LlamaContextPool* pool, llama_context* ctx);
    ~ContextGuard();
    ContextGuard(const ContextGuard&) = delete;
    ContextGuard& operator=(const ContextGuard&) = delete;
    ContextGuard(ContextGuard&& other) noexcept;
    ContextGuard& operator=(ContextGuard&& other) noexcept;

    llama_context* operator->() { return ctx_; }
    llama_context* get() { return ctx_; }

private:
    LlamaContextPool* pool_;
    llama_context* ctx_;
};

class LlamaContextPool {
public:
    LlamaContextPool(const Settings& settings, llama_model* model);
    ~LlamaContextPool();
    ContextGuard acquire();
    void release(llama_context* ctx);
    size_t size() const { return max_size_; }
private:
    llama_model* model_;
    const Settings& settings_;
    size_t max_size_;
    std::queue<llama_context*> pool_;
    std::mutex mutex_;
    std::condition_variable cv_;
    void initialize_contexts();
};

class LLMEngine {
public:
    explicit LLMEngine(Settings& settings);
    ~LLMEngine();
    LLMEngine(const LLMEngine&) = delete;
    LLMEngine& operator=(const LLMEngine&) = delete;

    void generate_stream(
        const sentiric::llm::v1::LLMLocalServiceGenerateStreamRequest& request,
        const std::function<void(const std::string&)>& on_token_callback,
        const std::function<bool()>& should_stop_callback,
        int32_t& prompt_tokens_out,
        int32_t& completion_tokens_out
    );

    bool is_model_loaded() const;

private:
    Settings& settings_;
    llama_model* model_ = nullptr;
    std::atomic<bool> model_loaded_{false};
    std::unique_ptr<LlamaContextPool> context_pool_;
};
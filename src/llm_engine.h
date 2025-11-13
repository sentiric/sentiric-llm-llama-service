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

class LlamaContextPool {
public:
    LlamaContextPool(llama_model* model, const Settings& settings, size_t pool_size);
    ~LlamaContextPool();

    llama_context* acquire();
    void release(llama_context* ctx);

private:
    llama_model* model_;
    const Settings& settings_;
    std::queue<llama_context*> pool_;
    std::mutex mutex_;
    std::condition_variable cv_;
};

class LLMEngine {
public:
    explicit LLMEngine(const Settings& settings);
    ~LLMEngine();

    LLMEngine(const LLMEngine&) = delete;
    LLMEngine& operator=(const LLMEngine&) = delete;

    void generate_stream(
        const sentiric::llm::v1::LocalGenerateStreamRequest& request,
        const std::function<void(const std::string&)>& on_token_callback,
        const std::function<bool()>& should_stop_callback
    );

    bool is_model_loaded() const;

private:
    llama_model* model_ = nullptr;
    std::atomic<bool> model_loaded_{false};
    Settings settings_;
    std::unique_ptr<LlamaContextPool> context_pool_;
};
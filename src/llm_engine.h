#pragma once

#include "config.h"
#include "core/prompt_formatter.h"
#include "core/context_pool.h"
#include "core/dynamic_batcher.h"
#include "llama.h"
#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <memory>
// DÜZELTME: Header değişti
#include "sentiric/llm/v1/llama.pb.h"
#include <prometheus/gauge.h>

class LLMEngine {
public:
    explicit LLMEngine(Settings& settings, prometheus::Gauge& active_contexts_gauge);
    ~LLMEngine();
    LLMEngine(const LLMEngine&) = delete;
    LLMEngine& operator=(const LLMEngine&) = delete;

    void process_single_request(std::shared_ptr<BatchedRequest> batched_request);

    DynamicBatcher* get_batcher() const { return batcher_.get(); }
    bool is_batching_enabled() const { return batcher_ != nullptr; }
    bool is_model_loaded() const;
    LlamaContextPool& get_context_pool() { return *context_pool_; }
    
    const Settings& get_settings() const { return settings_; }

private:
    void process_batch(std::vector<std::shared_ptr<BatchedRequest>>& batch);
    void execute_single_request(std::shared_ptr<BatchedRequest> req_ptr);

    Settings& settings_;
    llama_model* model_ = nullptr;
    std::atomic<bool> model_loaded_{false};
    std::unique_ptr<LlamaContextPool> context_pool_;
    std::unique_ptr<PromptFormatter> formatter_;
    prometheus::Gauge& active_contexts_gauge_;
    
    std::unique_ptr<DynamicBatcher> batcher_;
};
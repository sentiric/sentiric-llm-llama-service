#pragma once

#include "config.h"
#include "core/context_pool.h"
#include "llama.h"
#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <memory>
#include "sentiric/llm/v1/local.pb.h"
#include <prometheus/gauge.h>

class LLMEngine {
public:
    explicit LLMEngine(Settings& settings, prometheus::Gauge& active_contexts_gauge);
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
    prometheus::Gauge& active_contexts_gauge_;
};
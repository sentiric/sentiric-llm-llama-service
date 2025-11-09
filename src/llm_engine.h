// src/llm_engine.h
#pragma once

#include "config.h"
#include "llama.h"
#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <memory>
#include <mutex>
#include "sentiric/llm/v1/local.pb.h"

class LLMEngine {
public:
    explicit LLMEngine(const Settings& settings);
    ~LLMEngine();

    LLMEngine(const LLMEngine&) = delete;
    LLMEngine& operator=(const LLMEngine&) = delete;

    void generate_stream(
        const sentiric::llm::v1::LocalGenerateStreamRequest& request,
        std::function<void(const std::string& token)> on_token_callback,
        std::function<bool()> should_stop_callback
    );

    bool is_model_loaded() const;
    const Settings& get_settings() const;

private:
    llama_model* model_ = nullptr;
    llama_context* ctx_ = nullptr;
    std::atomic<bool> model_loaded_{false};
    Settings settings_;
    std::mutex generation_mutex_;
};
#pragma once

#include "config.h"
#include "llama.h"
#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <memory>
#include <mutex> // Eşzamanlılık için mutex eklendi

class LLMEngine {
public:
    explicit LLMEngine(const Settings& settings);
    ~LLMEngine();

    LLMEngine(const LLMEngine&) = delete;
    LLMEngine& operator=(const LLMEngine&) = delete;

    void generate_stream(
        const std::string& prompt,
        std::function<void(const std::string& token)> on_token_callback,
        float temperature,
        int top_k,
        float top_p
    );

    bool is_model_loaded() const;

private:
    llama_model* model_ = nullptr;
    llama_context* ctx_ = nullptr;
    std::atomic<bool> model_loaded_{false};
    Settings settings_;
    std::mutex generation_mutex_; // KRİTİK: Thread güvenliği için mutex
};
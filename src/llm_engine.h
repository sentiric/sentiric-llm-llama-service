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
    llama_context* ctx_ = nullptr; // TEK BİR CONTEXT TUTACAĞIZ, HAVUZ ŞİMDİLİK DEVRE DIŞI
    std::atomic<bool> model_loaded_{false};
    const Settings& settings_;
    std::mutex engine_mutex_; // EŞZAMANLI İSTEKLERİ KİLİTLEMEK İÇİN
};
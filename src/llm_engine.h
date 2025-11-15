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

// İleri bildirim
class LlamaContextPool;

/**
 * @brief ContextGuard: RAII deseni ile LlamaContext yönetimini sağlar.
 */
class ContextGuard {
public:
    ContextGuard(LlamaContextPool* pool, llama_context* ctx);
    ~ContextGuard(); // Tanımı .cpp dosyasında

    // Kopyalama yasak
    ContextGuard(const ContextGuard&) = delete;
    ContextGuard& operator=(const ContextGuard&) = delete;

    // Taşıma (move) operasyonları
    ContextGuard(ContextGuard&& other) noexcept;
    ContextGuard& operator=(ContextGuard&& other) noexcept;
    
    llama_context* operator->() const { return ctx_; }
    llama_context* get() const { return ctx_; }

private:
    LlamaContextPool* pool_;
    llama_context* ctx_;
};

/**
 * @brief LlamaContextPool: Eşzamanlı istekleri işlemek için llama_context'leri yönetir.
 */
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

/**
 * @brief LLMEngine: Tüm LLM operasyonlarının ana arayüzü.
 */
class LLMEngine {
public:
    // DEĞİŞİKLİK: Constructor artık const olmayan bir referans alıyor.
    // Bu, motorun kendi içindeki settings nesnesini (örneğin model_path)
    // güncelleyebilmesini sağlar.
    explicit LLMEngine(Settings& settings);
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
    // DEĞİŞİKLİK: const kaldırıldı, artık bu bir referanstır.
    Settings& settings_;
    llama_model* model_ = nullptr;
    std::atomic<bool> model_loaded_{false};
    std::unique_ptr<LlamaContextPool> context_pool_;
};
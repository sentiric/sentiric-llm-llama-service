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
#include <shared_mutex>
#include <map>
#include "sentiric/llm/v1/llama.pb.h"
#include <prometheus/gauge.h>

class LLMEngine {
public:
    explicit LLMEngine(Settings& settings, prometheus::Gauge& active_contexts_gauge);
    ~LLMEngine();
    LLMEngine(const LLMEngine&) = delete;
    LLMEngine& operator=(const LLMEngine&) = delete;

    void process_single_request(std::shared_ptr<BatchedRequest> batched_request);
    bool reload_model(const std::string& profile_name);
    
    bool update_hardware_config(int gpu_layers, int context_size, bool kv_offload);

    // [GÜNCELLENDİ] LoRA adaptörünü Context seviyesinde uygular
    // Dönüş değeri: Adaptör başarıyla uygulandıysa true, hata varsa false
    bool apply_lora_to_context(llama_context* ctx, const std::string& lora_adapter_id);

    // [YENİ] Context üzerindeki tüm adaptörleri temizler
    void clear_lora_from_context(llama_context* ctx);

    DynamicBatcher* get_batcher() const { return batcher_.get(); }
    bool is_batching_enabled() const { return batcher_ != nullptr; }
    bool is_model_loaded() const;
    LlamaContextPool& get_context_pool() { return *context_pool_; }
    const Settings& get_settings() const { return settings_; }

private:
    void process_batch(std::vector<std::shared_ptr<BatchedRequest>>& batch);
    void execute_single_request(std::shared_ptr<BatchedRequest> req_ptr);
    bool internal_reload_model(); 

    std::vector<llama_token> tokenize_and_truncate(std::shared_ptr<BatchedRequest> req_ptr, const std::string& formatted_prompt);
    bool decode_prompt(llama_context* ctx, ContextGuard& guard, const std::vector<llama_token>& prompt_tokens, std::shared_ptr<BatchedRequest> req_ptr);
    void generate_response(llama_context* ctx, const std::vector<llama_token>& prompt_tokens, std::shared_ptr<BatchedRequest> req_ptr);

    // [YENİ] LoRA Adapter Cache Yönetimi
    struct llama_adapter_lora* get_or_load_adapter(const std::string& lora_id);
    void clear_adapter_cache();

    Settings settings_;
    llama_model* model_ = nullptr;
    std::atomic<bool> model_loaded_{false};
    
    // [GÜNCELLENDİ] LoRA Cache
    // Adapter ID -> Pointer mapping. Pointer'lar model_ free edildiğinde otomatik geçersiz kalır,
    // ancak manuel free gerekirse burada tutuyoruz.
    std::map<std::string, struct llama_adapter_lora*> lora_cache_;
    std::mutex lora_mutex_;

    std::unique_ptr<LlamaContextPool> context_pool_;
    std::unique_ptr<PromptFormatter> formatter_;
    std::unique_ptr<DynamicBatcher> batcher_;
    
    prometheus::Gauge& active_contexts_gauge_;
    mutable std::shared_mutex model_mutex_;
};
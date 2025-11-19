#include "llm_engine.h"
#include "spdlog/spdlog.h"
#include "model_manager.h"
#include "common.h"
#include <stdexcept>
#include <time.h>
#include <algorithm>
#include <vector>

// RAII
struct LlamaBatchGuard {
    llama_batch batch;
    LlamaBatchGuard(int32_t n_tokens, int32_t embd, int32_t n_seq_max) : batch(llama_batch_init(n_tokens, embd, n_seq_max)) {}
    ~LlamaBatchGuard() { if (batch.token) llama_batch_free(batch); }
};

struct LlamaSamplerGuard {
    llama_sampler* sampler;
    LlamaSamplerGuard(llama_sampler_chain_params params) : sampler(llama_sampler_chain_init(params)) {}
    ~LlamaSamplerGuard() { if (sampler) llama_sampler_free(sampler); }
};

LLMEngine::LLMEngine(Settings& settings, prometheus::Gauge& active_contexts_gauge)
    : settings_(settings), active_contexts_gauge_(active_contexts_gauge) {
    
    spdlog::info("🚀 Initializing LLM Engine (Queue Mode)...");
    settings_.model_path = ModelManager::ensure_model_is_ready(settings_);

    llama_backend_init();
    llama_numa_init(settings_.numa_strategy);

    llama_model_params model_params = llama_model_default_params();
    model_params.n_gpu_layers = settings_.n_gpu_layers;
    model_params.use_mmap = settings_.use_mmap;

    model_ = llama_model_load_from_file(settings_.model_path.c_str(), model_params);
    if (!model_) throw std::runtime_error("Model load failed.");

    char arch_buffer[64] = {0};
    llama_model_meta_val_str(model_, "general.architecture", arch_buffer, sizeof(arch_buffer));
    formatter_ = create_formatter_for_model(arch_buffer[0] ? arch_buffer : "unknown");

    context_pool_ = std::make_unique<LlamaContextPool>(settings_, model_, active_contexts_gauge_);
    model_loaded_ = true;

    if (settings.enable_dynamic_batching && settings.max_batch_size > 1) {
        batcher_ = std::make_unique<DynamicBatcher>(settings.max_batch_size, std::chrono::milliseconds(settings.batch_timeout_ms));
        batcher_->batch_processing_callback = [this](std::vector<std::shared_ptr<BatchedRequest>>& batch) {
            this->process_batch(batch);
        };
    }
}

LLMEngine::~LLMEngine() {
    if (batcher_) batcher_->stop();
    context_pool_.reset();
    if (model_) llama_model_free(model_);
    llama_backend_free();
}

bool LLMEngine::is_model_loaded() const { return model_loaded_.load(); }

void LLMEngine::process_single_request(std::shared_ptr<BatchedRequest> batched_request) {
    std::vector<std::shared_ptr<BatchedRequest>> single_batch;
    single_batch.push_back(batched_request);
    process_batch(single_batch);
}

void LLMEngine::process_batch(std::vector<std::shared_ptr<BatchedRequest>>& batch) {
    if (batch.empty()) return;

    spdlog::info("Processing batch of {} requests (Thread ID: {})", batch.size(), std::hash<std::thread::id>{}(std::this_thread::get_id()));

    for (auto& req_ptr : batch) {
        try {
            auto& request = req_ptr->request;
            const std::string formatted_prompt = formatter_->format(request);
            const auto* vocab = llama_model_get_vocab(model_);
            
            // Tokenizasyon (Daha güvenli buffer yönetimi ile)
            std::vector<llama_token> prompt_tokens(formatted_prompt.length() + 64); 
            int n_tokens = llama_tokenize(vocab, formatted_prompt.c_str(), formatted_prompt.length(), prompt_tokens.data(), prompt_tokens.size(), true, true);
            if (n_tokens < 0) {
                prompt_tokens.resize(-n_tokens);
                n_tokens = llama_tokenize(vocab, formatted_prompt.c_str(), formatted_prompt.length(), prompt_tokens.data(), prompt_tokens.size(), true, true);
            }
            if (n_tokens < 0) throw std::runtime_error("Tokenization failed");
            
            prompt_tokens.resize(n_tokens);
            req_ptr->prompt_tokens = n_tokens;

            // Context Edinme
            ContextGuard guard = context_pool_->acquire(prompt_tokens);
            llama_context* ctx = guard.get();
            size_t matched_len = guard.get_matched_tokens();

            // KV Cache Temizliği
            if (matched_len < prompt_tokens.size()) {
                llama_memory_seq_rm(llama_get_memory(ctx), -1, matched_len, -1);
            } else if (matched_len == prompt_tokens.size() && matched_len > 0) {
                matched_len--; 
                llama_memory_seq_rm(llama_get_memory(ctx), -1, matched_len, -1);
            }
            
            // Prompt Decode
            size_t tokens_to_process = prompt_tokens.size() - matched_len;
            if (tokens_to_process > 0) {
                LlamaBatchGuard prompt_batch(tokens_to_process, 0, 1);
                for(size_t i = 0; i < tokens_to_process; ++i) {
                    common_batch_add(prompt_batch.batch, prompt_tokens[matched_len + i], matched_len + i, {0}, false);
                }
                prompt_batch.batch.logits[prompt_batch.batch.n_tokens - 1] = true;
                if (llama_decode(ctx, prompt_batch.batch) != 0) throw std::runtime_error("Prompt decode failed");
            }

            // Sampler
            const auto& params = request.params();
            LlamaSamplerGuard sampler_guard(llama_sampler_chain_default_params());
            llama_sampler* sampler_chain = sampler_guard.sampler;
            
            llama_sampler_chain_add(sampler_chain, llama_sampler_init_top_k(params.has_top_k() ? params.top_k() : settings_.default_top_k));
            llama_sampler_chain_add(sampler_chain, llama_sampler_init_temp(params.has_temperature() ? params.temperature() : settings_.default_temperature));
            llama_sampler_chain_add(sampler_chain, llama_sampler_init_dist(time(NULL)));

            int n_decoded = 0;
            std::vector<llama_token> final_tokens = prompt_tokens; 
            llama_pos n_past = prompt_tokens.size();

            while (n_decoded < settings_.default_max_tokens) {
                if (req_ptr->should_stop_callback && req_ptr->should_stop_callback()) {
                    req_ptr->finish_reason = "cancelled"; break;
                }

                llama_token new_token_id = llama_sampler_sample(sampler_chain, ctx, -1);
                llama_sampler_accept(sampler_chain, new_token_id);
                if (llama_vocab_is_eog(vocab, new_token_id)) { req_ptr->finish_reason = "stop"; break; }

                char piece_buf[64] = {0};
                int n_piece = llama_token_to_piece(vocab, new_token_id, piece_buf, sizeof(piece_buf), 0, true);
                if (n_piece > 0) {
                    std::string token_str(piece_buf, n_piece);
                    
                    // 1. gRPC Callback (Varsa çağır)
                    if (req_ptr->on_token_callback) {
                         if (!req_ptr->on_token_callback(token_str)) {
                             req_ptr->finish_reason = "cancelled_by_client"; break;
                         }
                    }
                    
                    // 2. HTTP Queue (Güvenli gönderim)
                    req_ptr->token_queue.push(token_str);
                }
                
                final_tokens.push_back(new_token_id);
                LlamaBatchGuard token_batch(1, 0, 1);
                common_batch_add(token_batch.batch, new_token_id, n_past++, {0}, true);
                if (llama_decode(ctx, token_batch.batch) != 0) throw std::runtime_error("Token decode failed");
                n_decoded++;
            }
            
            req_ptr->completion_tokens = n_decoded;
            if (req_ptr->finish_reason.empty()) req_ptr->finish_reason = "length";

            // Güvenli çıkış
            guard.release_early(final_tokens);

        } catch(const std::exception& e) {
            spdlog::error("Request processing error: {}", e.what());
            req_ptr->finish_reason = "error";
        }
    }
}
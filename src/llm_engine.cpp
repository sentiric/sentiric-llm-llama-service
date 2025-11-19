// src/llm_engine.cpp
#include "llm_engine.h"
#include "spdlog/spdlog.h"
#include "model_manager.h"
#include "common.h"
#include <stdexcept>
#include <time.h>
#include <algorithm>
#include <string>
#include <vector>
#include <map>

// RAII sarmalayıcıları, kaynakların otomatik olarak serbest bırakılmasını garanti eder.
struct LlamaBatchGuard {
    llama_batch batch;
    LlamaBatchGuard(int32_t n_tokens, int32_t embd, int32_t n_seq_max)
        : batch(llama_batch_init(n_tokens, embd, n_seq_max)) {}
    ~LlamaBatchGuard() {
        if (batch.token) {
            llama_batch_free(batch);
        }
    }
};

struct LlamaSamplerGuard {
    llama_sampler* sampler;
    LlamaSamplerGuard(llama_sampler_chain_params params)
        : sampler(llama_sampler_chain_init(params)) {}
    ~LlamaSamplerGuard() {
        if (sampler) {
            llama_sampler_free(sampler);
        }
    }
};


LLMEngine::LLMEngine(Settings& settings, prometheus::Gauge& active_contexts_gauge)
    : settings_(settings), active_contexts_gauge_(active_contexts_gauge) {

    spdlog::info("Initializing LLM Engine...");
    try {
        settings_.model_path = ModelManager::ensure_model_is_ready(settings_);
        spdlog::info("Model successfully located at: {}", settings_.model_path);
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Model preparation failed: ") + e.what());
    }

    llama_backend_init();
    llama_numa_init(settings_.numa_strategy);

    llama_model_params model_params = llama_model_default_params();
    model_params.n_gpu_layers = settings_.n_gpu_layers;
    model_params.use_mmap = settings_.use_mmap;

    model_ = llama_model_load_from_file(settings_.model_path.c_str(), model_params);
    if (!model_) throw std::runtime_error("Model loading failed from path: " + settings_.model_path);

    try {
        char arch_buffer[64];
        llama_model_meta_val_str(model_, "general.architecture", arch_buffer, sizeof(arch_buffer));
        std::string model_architecture = arch_buffer;
        formatter_ = create_formatter_for_model(model_architecture);
    } catch (const std::exception& e) {
        spdlog::error("Could not determine model architecture for formatter selection: {}. Falling back to default.", e.what());
        formatter_ = create_formatter_for_model("unknown");
    }

    try {
        context_pool_ = std::make_unique<LlamaContextPool>(settings_, model_, active_contexts_gauge_);
        model_loaded_ = true;
    } catch (const std::exception& e) {
        llama_model_free(model_);
        throw std::runtime_error(std::string("Context pool initialization failed: ") + e.what());
    }

    if (settings.enable_dynamic_batching && settings.max_batch_size > 1) {
        spdlog::info("Dynamic batching enabled (max_size: {}, timeout: {}ms).", settings.max_batch_size, settings.batch_timeout_ms);
        batcher_ = std::make_unique<DynamicBatcher>(settings.max_batch_size, std::chrono::milliseconds(settings.batch_timeout_ms));
        batcher_->batch_processing_callback = [this](std::vector<std::shared_ptr<BatchedRequest>>& batch) {
            this->process_batch(batch);
        };
    } else {
        spdlog::info("Dynamic batching is disabled.");
        batcher_ = nullptr;
    }

    spdlog::info("✅ LLM Engine initialized successfully.");
}

LLMEngine::~LLMEngine() {
    if (batcher_) {
        batcher_->stop();
    }
    context_pool_.reset();
    if (model_) llama_model_free(model_);
    llama_backend_free();
    spdlog::info("LLM Engine shut down.");
}

bool LLMEngine::is_model_loaded() const {
    return model_loaded_.load();
}

void LLMEngine::process_single_request(std::shared_ptr<BatchedRequest> batched_request) {
    std::vector<std::shared_ptr<BatchedRequest>> single_batch;
    single_batch.push_back(batched_request);
    process_batch(single_batch);
}

void LLMEngine::process_batch(std::vector<std::shared_ptr<BatchedRequest>>& batch) {
    if (batch.empty()) return;

    spdlog::debug("[Batch] Processing batch of size: {}", batch.size());
    ContextGuard guard = context_pool_->acquire();
    llama_context* ctx = guard.get();

    for (auto& req_ptr : batch) {
        try {
            llama_memory_seq_rm(llama_get_memory(ctx), -1, -1, -1);

            auto& request = req_ptr->request;
            const std::string formatted_prompt = formatter_->format(request);
            const auto* vocab = llama_model_get_vocab(model_);
            std::vector<llama_token> prompt_tokens;
            prompt_tokens.resize(formatted_prompt.length() + 16);
            int n_tokens = llama_tokenize(vocab, formatted_prompt.c_str(), formatted_prompt.length(), prompt_tokens.data(), prompt_tokens.size(), false, true);
            if (n_tokens < 0) { throw std::runtime_error("Tokenization failed."); }
            prompt_tokens.resize(n_tokens);
            req_ptr->prompt_tokens = n_tokens;

            const uint32_t n_ctx = llama_n_ctx(ctx);
            if ((uint32_t)n_tokens > n_ctx) {
                const int n_keep = n_ctx / 2;
                prompt_tokens.erase(prompt_tokens.begin(), prompt_tokens.begin() + (n_tokens - n_keep));
                n_tokens = prompt_tokens.size();
            }

            LlamaBatchGuard prompt_batch(n_tokens, 0, 1);
            for(int i = 0; i < n_tokens; ++i) {
                common_batch_add(prompt_batch.batch, prompt_tokens[i], (llama_pos)i, {0}, false);
            }
            prompt_batch.batch.logits[prompt_batch.batch.n_tokens - 1] = true;

            if (llama_decode(ctx, prompt_batch.batch) != 0) {
                throw std::runtime_error("llama_decode failed on prompt");
            }

            const auto& params = request.params();
            bool use_request_params = request.has_params();
            float temperature = use_request_params && params.has_temperature() ? params.temperature() : settings_.default_temperature;
            int32_t top_k = use_request_params && params.has_top_k() ? params.top_k() : settings_.default_top_k;
            float top_p = use_request_params && params.has_top_p() ? params.top_p() : settings_.default_top_p;
            float repeat_penalty = use_request_params && params.has_repetition_penalty() ? params.repetition_penalty() : settings_.default_repeat_penalty;

            // ===== START OF FINAL, EVIDENCE-BASED SAMPLER CHAIN FIX =====
            LlamaSamplerGuard sampler_guard(llama_sampler_chain_default_params());
            llama_sampler* sampler_chain = sampler_guard.sampler;

            // Örnekleme Zinciri: llama.h @ 92bb442 ile %100 uyumlu
            // 1. Cezalar (Logit'leri değiştirir)
            llama_sampler_chain_add(sampler_chain, llama_sampler_init_penalties(n_ctx, repeat_penalty, 0.0f, 0.0f));
            // 2. Filtreler (Aday havuzunu daraltır)
            llama_sampler_chain_add(sampler_chain, llama_sampler_init_top_k(top_k));
            llama_sampler_chain_add(sampler_chain, llama_sampler_init_top_p(top_p, 1));
            // 3. Değiştiriciler (Olasılıkları ayarlar)
            llama_sampler_chain_add(sampler_chain, llama_sampler_init_temp(temperature));
            // 4. Nihai Seçici (Kalan adaylar arasından olasılık dağılımına göre seçim yapar)
            // BU EKSIKTI VE ÇÖKMEYE NEDEN OLUYORDU.
            llama_sampler_chain_add(sampler_chain, llama_sampler_init_dist(time(NULL)));
            // ===== END OF FINAL, EVIDENCE-BASED SAMPLER CHAIN FIX =====

            int n_decoded = 0;
            llama_pos n_past = n_tokens;
            int32_t max_tokens = use_request_params && params.has_max_new_tokens() ? params.max_new_tokens() : settings_.default_max_tokens;

            while (n_past < (llama_pos)n_ctx && n_decoded < max_tokens) {
                if (req_ptr->should_stop_callback && req_ptr->should_stop_callback()) {
                    req_ptr->finish_reason = "cancelled";
                    break;
                }

                llama_token new_token_id = llama_sampler_sample(sampler_chain, ctx, -1);
                llama_sampler_accept(sampler_chain, new_token_id);

                if (llama_vocab_is_eog(vocab, new_token_id)) {
                    req_ptr->finish_reason = "stop";
                    break;
                }

                char piece_buf[64] = {0};
                int n_piece = llama_token_to_piece(vocab, new_token_id, piece_buf, sizeof(piece_buf), 0, true);

                if (n_piece > 0) {
                    std::string token_str(piece_buf, n_piece);
                    if (req_ptr->on_token_callback && !req_ptr->on_token_callback(token_str)) {
                         req_ptr->finish_reason = "cancelled_by_client";
                         break;
                    }
                }

                LlamaBatchGuard token_batch(1, 0, 1);
                common_batch_add(token_batch.batch, new_token_id, n_past, {0}, true);

                if (llama_decode(ctx, token_batch.batch) != 0) {
                    throw std::runtime_error("Failed to decode next token");
                }
                n_past++;
                n_decoded++;
            }

            if (req_ptr->finish_reason.empty() || req_ptr->finish_reason == "stop") {
                 req_ptr->finish_reason = (n_decoded == max_tokens) ? "length" : "stop";
            }

            req_ptr->completion_tokens = n_decoded;

        } catch(...) {
            req_ptr->finish_reason = "error";
            throw;
        }
    }
    spdlog::debug("[Batch] Finished processing batch.");
}
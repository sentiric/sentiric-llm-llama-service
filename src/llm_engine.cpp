// src/llm_engine.cpp
#include "llm_engine.h"
#include "spdlog/spdlog.h"
#include "model_manager.h"
#include "common.h"
// #include "core/grammar_parser.h" <-- KALDIRILDI
#include <stdexcept>
#include <time.h>
#include <algorithm>
#include <vector>
#include <thread>

// RAII Wrappers
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
    
    spdlog::info("🚀 Initializing LLM Engine (Parallel Batching Mode)...");
    settings_.model_path = ModelManager::ensure_model_is_ready(settings_);

    llama_backend_init();
    llama_numa_init(settings_.numa_strategy);

    llama_model_params model_params = llama_model_default_params();
    model_params.n_gpu_layers = settings_.n_gpu_layers;
    model_params.use_mmap = settings_.use_mmap;

    model_ = llama_model_load_from_file(settings_.model_path.c_str(), model_params);
    if (!model_) throw std::runtime_error("Model load failed.");

    // --- OTOMATİK FORMAT ALGILAMA ---
    char arch_buffer[64] = {0};
    llama_model_meta_val_str(model_, "general.architecture", arch_buffer, sizeof(arch_buffer));
    std::string arch = arch_buffer[0] ? arch_buffer : "unknown";
    
    spdlog::info("🔍 Detected Model Architecture: {}", arch);
    
    formatter_ = create_formatter_for_model(arch);

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
    execute_single_request(batched_request);
}

void LLMEngine::process_batch(std::vector<std::shared_ptr<BatchedRequest>>& batch) {
    if (batch.empty()) return;

    std::vector<std::thread> threads;
    threads.reserve(batch.size());

    spdlog::info("🚀 Dispatching {} requests in parallel...", batch.size());

    for (auto& req_ptr : batch) {
        threads.emplace_back([this, req_ptr]() {
            this->execute_single_request(req_ptr);
        });
    }

    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }
    
    spdlog::info("✅ Batch processing completed.");
}

// --- YARDIMCI FONKSİYONLAR ---
void safe_kv_clear(llama_context* ctx) {
    llama_memory_seq_rm(llama_get_memory(ctx), -1, 0, -1);
}

bool safe_kv_seq_rm(llama_context* ctx, llama_seq_id seq, llama_pos p0, llama_pos p1) {
    return llama_memory_seq_rm(llama_get_memory(ctx), seq, p0, p1);
}

void LLMEngine::execute_single_request(std::shared_ptr<BatchedRequest> req_ptr) {
    try {
        auto& request = req_ptr->request;
        const std::string formatted_prompt = formatter_->format(request);
        const auto* vocab = llama_model_get_vocab(model_);
        
        // 1. Tokenizasyon
        std::vector<llama_token> prompt_tokens(formatted_prompt.length() + 64); 
        int n_tokens = llama_tokenize(vocab, formatted_prompt.c_str(), formatted_prompt.length(), prompt_tokens.data(), prompt_tokens.size(), true, true);
        
        if (n_tokens < 0) {
            prompt_tokens.resize(-n_tokens);
            n_tokens = llama_tokenize(vocab, formatted_prompt.c_str(), formatted_prompt.length(), prompt_tokens.data(), prompt_tokens.size(), true, true);
        }
        if (n_tokens < 0) throw std::runtime_error("Tokenization failed");
        
        prompt_tokens.resize(n_tokens);
        
        // --- TRUNCATION LOGIC (YENİ: Input Protection) ---
        // Context boyutu aşılırsa, sondan koruyarak kes (Keep Last).
        // Modelin cevap üretmesi için de yer bırakmalıyız.
        uint32_t max_context = settings_.context_size;
        uint32_t max_gen = settings_.default_max_tokens;
        
        // Güvenlik marjı (buffer) ile birlikte maksimum prompt boyutu
        uint32_t safe_prompt_limit = (max_context > max_gen + 64) ? (max_context - max_gen - 64) : (max_context / 2);
        
        if (prompt_tokens.size() > safe_prompt_limit) {
            spdlog::warn("⚠️ Input too large ({} tokens). Truncating to last {} tokens to fit context.", 
                         prompt_tokens.size(), safe_prompt_limit);
                         
            // Basit "Keep Last" stratejisi (RAG için genellikle soru sondadır)
            // Sistemin en azından çalışmasını sağlar.
            std::vector<llama_token> truncated;
            truncated.reserve(safe_prompt_limit);
            
            // Iterator aritmetiği ile son 'safe_prompt_limit' kadarını al
            auto start_it = prompt_tokens.end() - safe_prompt_limit;
            truncated.assign(start_it, prompt_tokens.end());
            
            prompt_tokens = std::move(truncated);
        }
        
        req_ptr->prompt_tokens = prompt_tokens.size();

        // 2. Context Edinme
        ContextGuard guard = context_pool_->acquire(prompt_tokens);
        llama_context* ctx = guard.get();
        size_t matched_len = guard.get_matched_tokens();

        // 3. KV Cache Temizliği
        if (matched_len < prompt_tokens.size()) {
            if (!safe_kv_seq_rm(ctx, 0, matched_len, -1)) {
                spdlog::error("Failed to remove KV cache sequence! Resetting context.");
                safe_kv_clear(ctx);
                matched_len = 0;
            }
        } else if (matched_len == 0) {
             safe_kv_clear(ctx);
        }
        
        // 4. Prompt Decode
        size_t tokens_to_process = prompt_tokens.size() - matched_len;
        
        if (tokens_to_process > 0) {
            int32_t n_batch = llama_n_batch(ctx);
            for (size_t i = 0; i < tokens_to_process; i += n_batch) {
                int32_t n_eval = (int32_t)tokens_to_process - i;
                if (n_eval > n_batch) n_eval = n_batch;
                
                LlamaBatchGuard prompt_batch(n_eval, 0, 1);
                for(int j = 0; j < n_eval; ++j) {
                    common_batch_add(prompt_batch.batch, prompt_tokens[matched_len + i + j], matched_len + i + j, {0}, false);
                }
                
                if (i + n_eval == tokens_to_process) {
                    prompt_batch.batch.logits[n_eval - 1] = true;
                }
                
                if (llama_decode(ctx, prompt_batch.batch) != 0) {
                    spdlog::error("Prompt decode failed at chunk {}/{}.", i, tokens_to_process);
                    safe_kv_clear(ctx);
                    throw std::runtime_error("Prompt decode failed.");
                }
            }
        }

        // 5. Sampler Hazırlığı
        const auto& params = request.params();
        LlamaSamplerGuard sampler_guard(llama_sampler_chain_default_params());
        llama_sampler* sampler_chain = sampler_guard.sampler;
        
        // --- GRAMMAR SAMPLER ENTEGRASYONU ---
        if (!req_ptr->grammar.empty()) {
            llama_sampler* grammar_sampler = llama_sampler_init_grammar(vocab, req_ptr->grammar.c_str(), "root");
            if (grammar_sampler) {
                llama_sampler_chain_add(sampler_chain, grammar_sampler);
                spdlog::info("✅ Grammar constraint applied.");
            } else {
                spdlog::error("❌ Failed to initialize grammar sampler.");
            }
        }

        llama_sampler_chain_add(sampler_chain, llama_sampler_init_top_k(params.has_top_k() ? params.top_k() : settings_.default_top_k));
        llama_sampler_chain_add(sampler_chain, llama_sampler_init_temp(params.has_temperature() ? params.temperature() : settings_.default_temperature));
        llama_sampler_chain_add(sampler_chain, llama_sampler_init_dist(time(NULL)));
        
        auto stop_sequences = formatter_->get_stop_sequences();

        // 6. Generation Döngüsü
        int n_decoded = 0;
        std::vector<llama_token> final_tokens = prompt_tokens; 
        llama_pos n_past = prompt_tokens.size();

        while (n_decoded < settings_.default_max_tokens) {
            if (req_ptr->should_stop_callback && req_ptr->should_stop_callback()) {
                req_ptr->finish_reason = "cancelled"; break;
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
                
                bool stop_triggered = false;
                for (const auto& seq : stop_sequences) {
                    if (token_str.find(seq) != std::string::npos) {
                        stop_triggered = true;
                        break;
                    }
                }
                if (stop_triggered) {
                    req_ptr->finish_reason = "stop";
                    break;
                }

                if (token_str.find("<unused") == std::string::npos) {
                     if (req_ptr->on_token_callback) {
                        if (!req_ptr->on_token_callback(token_str)) {
                            req_ptr->finish_reason = "cancelled_by_client"; break;
                        }
                     }
                     req_ptr->token_queue.push(token_str);
                }
            }
            
            final_tokens.push_back(new_token_id);
            
            LlamaBatchGuard token_batch(1, 0, 1);
            common_batch_add(token_batch.batch, new_token_id, n_past, {0}, true);
            
            if (llama_decode(ctx, token_batch.batch) != 0) {
                 req_ptr->finish_reason = "length";
                 break;
            }
            
            n_past++;
            n_decoded++;
        }
        
        req_ptr->completion_tokens = n_decoded;
        if (req_ptr->finish_reason.empty()) req_ptr->finish_reason = "length";

        guard.release_early(final_tokens);

    } catch(const std::exception& e) {
        spdlog::error("Request processing error: {}", e.what());
        req_ptr->finish_reason = "error";
    }
}
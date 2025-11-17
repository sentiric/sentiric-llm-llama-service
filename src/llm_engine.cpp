#include "llm_engine.h"
#include "core/dynamic_batcher.h"
#include "spdlog/spdlog.h"
#include "model_manager.h"
#include "common.h"
#include "core/prompt_formatter.h"
#include <stdexcept>
#include <time.h>
#include <algorithm>
#include <string>
#include <vector>

// Global batcher - geçici olarak devre dışı
// static std::unique_ptr<DynamicBatcher> g_batcher;

LLMEngine::LLMEngine(Settings& settings, prometheus::Gauge& active_contexts_gauge) 
    : settings_(settings), active_contexts_gauge_(active_contexts_gauge) {
    
    spdlog::info("Initializing LLM Engine with advanced performance settings...");
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
        context_pool_ = std::make_unique<LlamaContextPool>(settings_, model_, active_contexts_gauge_);
        model_loaded_ = true;
        
        // Dynamic Batcher geçici olarak devre dışı - stabilite için
        /*
        if (settings_.enable_dynamic_batching && settings_.n_threads > 1) {
            g_batcher = std::make_unique<DynamicBatcher>(
                std::min(settings_.max_batch_size, static_cast<size_t>(settings_.n_threads)),
                std::chrono::milliseconds(settings_.batch_timeout_ms)
            );
            
            g_batcher->batch_processing_callback = [this](const std::vector<BatchedRequest>& batch) {
                this->process_batch(batch);
            };
            spdlog::info("Dynamic batching enabled: max_batch_size={}, timeout={}ms", 
                        settings_.max_batch_size, settings_.batch_timeout_ms);
        } else {
            spdlog::info("Dynamic batching disabled");
        }
        */
        
    } catch (const std::exception& e) { 
        llama_model_free(model_); 
        throw std::runtime_error(std::string("Context pool initialization failed: ") + e.what()); 
    }
    spdlog::info("✅ LLM Engine initialized successfully.");
}

LLMEngine::~LLMEngine() {
    /*
    if (g_batcher) {
        g_batcher->stop();
        g_batcher.reset();
    }
    */
    context_pool_.reset();
    if (model_) llama_model_free(model_);
    llama_backend_free();
    spdlog::info("LLM Engine shut down.");
}

// UTF-8 Validation fonksiyonu - GÜVENLİ VERSİYON
bool LLMEngine::is_valid_utf8(const std::string& str) {
    int i = 0;
    int len = str.length();
    
    while (i < len) {
        unsigned char c = str[i];
        
        if (c <= 0x7F) {
            // 1-byte character (0xxxxxxx)
            i++;
        } else if ((c & 0xE0) == 0xC0) {
            // 2-byte character (110xxxxx)
            if (i + 1 >= len || (str[i+1] & 0xC0) != 0x80) return false;
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            // 3-byte character (1110xxxx)
            if (i + 2 >= len || (str[i+1] & 0xC0) != 0x80 || (str[i+2] & 0xC0) != 0x80) return false;
            i += 3;
        } else if ((c & 0xF8) == 0xF0) {
            // 4-byte character (11110xxx)
            if (i + 3 >= len || (str[i+1] & 0xC0) != 0x80 || 
                (str[i+2] & 0xC0) != 0x80 || (str[i+3] & 0xC0) != 0x80) return false;
            i += 4;
        } else {
            // Invalid UTF-8
            return false;
        }
    }
    return true;
}

void LLMEngine::process_batch(const std::vector<BatchedRequest>& batch) {
    if (batch.empty()) return;
    
    spdlog::debug("Processing batch of {} requests", batch.size());
    
    // Basit implementasyon: Her request'i ayrı işle
    for (const auto& batched_request : batch) {
        try {
            int32_t prompt_tokens = 0;
            int32_t completion_tokens = 0;
            
            this->generate_stream(
                batched_request.request,
                batched_request.on_token_callback,
                batched_request.should_stop_callback,
                prompt_tokens,
                completion_tokens
            );
            
        } catch (const std::exception& e) {
            spdlog::error("Failed to process batched request: {}", e.what());
        }
    }
}

bool LLMEngine::is_model_loaded() const {
    return model_loaded_.load();
}

void LLMEngine::generate_stream(
    const sentiric::llm::v1::LLMLocalServiceGenerateStreamRequest& request,
    const std::function<void(const std::string&)>& on_token_callback,
    const std::function<bool()>& should_stop_callback,
    int32_t& prompt_tokens_out,
    int32_t& completion_tokens_out) {
    
    // Dynamic batching geçici olarak devre dışı - stabilite için
    /*
    if (g_batcher && settings_.enable_dynamic_batching) {
        auto future = g_batcher->add_request(
            request, on_token_callback, should_stop_callback,
            prompt_tokens_out, completion_tokens_out
        );
        future.get();
        return;
    }
    */
    
    ContextGuard guard = context_pool_->acquire();
    llama_context* ctx = guard.get();
    
    const std::string formatted_prompt = PromptFormatter::format(request);
    const auto* vocab = llama_model_get_vocab(model_);
    std::vector<llama_token> prompt_tokens;
    prompt_tokens.resize(formatted_prompt.length() + 16);
    int n_tokens = llama_tokenize(vocab, formatted_prompt.c_str(), formatted_prompt.length(), prompt_tokens.data(), prompt_tokens.size(), false, true);
    if (n_tokens < 0) { throw std::runtime_error("Tokenization failed."); }
    prompt_tokens.resize(n_tokens);
    prompt_tokens_out = n_tokens;

    const uint32_t n_ctx = llama_n_ctx(ctx);
    if ((uint32_t)n_tokens > n_ctx) {
        spdlog::warn("Prompt size ({}) exceeds context size ({}). Applying context shifting.", n_tokens, n_ctx);
        const int n_keep = n_ctx / 2;
        prompt_tokens.erase(prompt_tokens.begin(), prompt_tokens.begin() + (n_tokens - n_keep));
    }

    llama_batch batch = llama_batch_init(prompt_tokens.size(), 0, 1);
    for(size_t i = 0; i < prompt_tokens.size(); ++i) { 
        common_batch_add(batch, prompt_tokens[i], (llama_pos)i, {0}, false); 
    }
    batch.logits[batch.n_tokens - 1] = true;
    if (llama_decode(ctx, batch) != 0) { 
        llama_batch_free(batch); 
        throw std::runtime_error("llama_decode failed on prompt"); 
    }
    
    const auto& params = request.params();
    const bool use_request_params = request.has_params();
    float temperature = use_request_params && params.has_temperature() ? params.temperature() : settings_.default_temperature;
    int32_t top_k = use_request_params && params.has_top_k() ? params.top_k() : settings_.default_top_k;
    float top_p = use_request_params && params.has_top_p() ? params.top_p() : settings_.default_top_p;
    float repeat_penalty = use_request_params && params.has_repetition_penalty() ? params.repetition_penalty() : settings_.default_repeat_penalty;

    llama_sampler_chain_params sparams = llama_sampler_chain_default_params();
    llama_sampler* sampler_chain = llama_sampler_chain_init(sparams);
    
    llama_sampler_chain_add(sampler_chain, llama_sampler_init_penalties(llama_n_ctx(ctx), repeat_penalty, 0.0f, 0.0f));
    llama_sampler_chain_add(sampler_chain, llama_sampler_init_top_k(top_k));
    llama_sampler_chain_add(sampler_chain, llama_sampler_init_top_p(top_p, 1));
    llama_sampler_chain_add(sampler_chain, llama_sampler_init_min_p(0.05f, 1));
    llama_sampler_chain_add(sampler_chain, llama_sampler_init_temp(temperature));
    llama_sampler_chain_add(sampler_chain, llama_sampler_init_dist(static_cast<uint32_t>(time(NULL))));

    auto stop_sequences = PromptFormatter::get_stop_sequences();
    std::string stop_buffer;
    int n_decoded = 0;
    llama_pos n_past = batch.n_tokens;
    int32_t max_tokens = use_request_params && params.has_max_new_tokens() ? params.max_new_tokens() : settings_.default_max_tokens;
    
    while (n_past < (llama_pos)n_ctx && n_decoded < max_tokens) {
        if (should_stop_callback()) { break; }

        if (n_past >= (llama_pos)n_ctx) {
            const llama_pos n_keep   = n_ctx / 2;
            const llama_pos n_discard = n_ctx - n_keep;
            
            llama_memory_seq_rm(llama_get_memory(ctx), 0, n_keep, -1);
            llama_memory_seq_add(llama_get_memory(ctx), 0, n_keep, n_past, -n_discard);

            n_past = n_keep;
            spdlog::debug("Context shifting applied. n_past reset to {}", n_past);
        }
        
        llama_token new_token_id = llama_sampler_sample(sampler_chain, ctx, -1);
        llama_sampler_accept(sampler_chain, new_token_id);
        
        if (llama_vocab_is_eog(vocab, new_token_id)) { break; }
        
        char piece_buf[64] = {0};
        int n_piece = llama_token_to_piece(vocab, new_token_id, piece_buf, sizeof(piece_buf), 0, true);
        
        if (n_piece > 0) {
            std::string token_str(piece_buf, n_piece);
            
            // UTF-8 VALIDATION - CRITICAL FIX
            if (!is_valid_utf8(token_str)) {
                spdlog::warn("Skipping invalid UTF-8 token (length: {})", n_piece);
                continue;
            }
            
            stop_buffer += token_str;
            std::string flush_str;
            bool stopping = false;
            
            for (const auto& stop_word : stop_sequences) {
                if (stop_buffer.length() >= stop_word.length() && 
                    stop_buffer.compare(stop_buffer.length() - stop_word.length(), stop_word.length(), stop_word) == 0) {
                    flush_str = stop_buffer.substr(0, stop_buffer.length() - stop_word.length());
                    stopping = true;
                    break;
                }
            }
            
            if (stopping) {
                if (!flush_str.empty()) on_token_callback(flush_str);
                break;
            }
            
            size_t max_stop_len = 32;
            if (stop_buffer.length() > max_stop_len) {
                flush_str = stop_buffer.substr(0, stop_buffer.length() - max_stop_len);
                stop_buffer = stop_buffer.substr(stop_buffer.length() - max_stop_len);
                on_token_callback(flush_str);
            }
        }
        
        llama_batch_free(batch);
        batch = llama_batch_init(1, 0, 1);
        common_batch_add(batch, new_token_id, n_past, {0}, true);
        if (llama_decode(ctx, batch) != 0) { 
            llama_batch_free(batch); 
            llama_sampler_free(sampler_chain); 
            throw std::runtime_error("Failed to decode next token"); 
        }
        n_past++;
        n_decoded++;
    }

    if (!stop_buffer.empty() && n_decoded > 0) {
        on_token_callback(stop_buffer);
    }
    
    completion_tokens_out = n_decoded;
    llama_batch_free(batch);
    llama_sampler_free(sampler_chain);
}
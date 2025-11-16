#include "llm_engine.h"
#include "spdlog/spdlog.h"
#include "model_manager.h"
#include "common.h"
#include "core/prompt_formatter.h"
#include <stdexcept>
#include <time.h>
#include <algorithm>
#include <string>
#include <vector>

LLMEngine::LLMEngine(Settings& settings) : settings_(settings) {
    spdlog::info("Initializing LLM Engine with advanced performance settings...");
    try {
        settings_.model_path = ModelManager::ensure_model_is_ready(settings_);
        spdlog::info("Model successfully located at: {}", settings_.model_path);
    } catch (const std::exception& e) { throw std::runtime_error(std::string("Model preparation failed: ") + e.what()); }
    
    llama_backend_init();
    llama_numa_init(settings_.numa_strategy);

    llama_model_params model_params = llama_model_default_params();
    model_params.n_gpu_layers = settings_.n_gpu_layers;
    model_params.use_mmap = settings_.use_mmap;

    model_ = llama_model_load_from_file(settings_.model_path.c_str(), model_params);
    if (!model_) throw std::runtime_error("Model loading failed from path: " + settings_.model_path);
    
    try {
        context_pool_ = std::make_unique<LlamaContextPool>(settings_, model_);
        model_loaded_ = true;
    } catch (const std::exception& e) { 
        llama_model_free(model_); 
        throw std::runtime_error(std::string("Context pool initialization failed: ") + e.what()); 
    }
    spdlog::info("✅ LLM Engine initialized successfully.");
}

LLMEngine::~LLMEngine() {
    context_pool_.reset();
    if (model_) llama_model_free(model_);
    llama_backend_free();
    spdlog::info("LLM Engine shut down.");
}

bool LLMEngine::is_model_loaded() const { return model_loaded_.load(); }

void LLMEngine::generate_stream(
    // --- DÜZELTME BURADA ---
    const sentiric::llm::v1::LLMLocalServiceGenerateStreamRequest& request,
    const std::function<void(const std::string&)>& on_token_callback,
    const std::function<bool()>& should_stop_callback,
    int32_t& prompt_tokens_out,
    int32_t& completion_tokens_out) {
    
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
    for(size_t i = 0; i < prompt_tokens.size(); ++i) { common_batch_add(batch, prompt_tokens[i], (llama_pos)i, {0}, false); }
    batch.logits[batch.n_tokens - 1] = true;
    if (llama_decode(ctx, batch) != 0) { llama_batch_free(batch); throw std::runtime_error("llama_decode failed on prompt"); }
    
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
            stop_buffer += token_str;
            std::string flush_str;
            bool stopping = false;
            for (const auto& stop_word : stop_sequences) {
                 if (stop_buffer.length() >= stop_word.length() && stop_buffer.compare(stop_buffer.length() - stop_word.length(), stop_word.length(), stop_word) == 0) {
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
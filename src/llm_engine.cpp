#include "llm_engine.h"
#include "spdlog/spdlog.h"
#include <stdexcept>
#include <vector>
#include <string>
#include <algorithm>
#include "model_manager.h"

// --- LLMEngine Implementasyonu ---
LLMEngine::LLMEngine(const Settings& settings) : settings_(settings) {
    spdlog::info("Initializing LLM Engine...");
    
    // ModelManager artık LLMEngine içinde çağrılmıyor, main.cpp'ye geri alıyoruz.
    // Bu, bağımlılıkları basitleştirir ve sorunu izole etmemize yardımcı olur.

    spdlog::info("Initializing llama.cpp backend...");
    llama_backend_init();

    llama_model_params model_params = llama_model_default_params();
    model_params.n_gpu_layers = settings.n_gpu_layers;
    
    // ÖNEMLİ: b7046'da bu fonksiyonun adı 'llama_model_load_from_file'
    model_ = llama_model_load_from_file(settings.model_path.c_str(), model_params);
    if (!model_) {
        throw std::runtime_error("Model loading failed from path: " + settings.model_path);
    }

    llama_context_params ctx_params = llama_context_default_params();
    ctx_params.n_ctx = settings.context_size;
    ctx_params.n_threads = settings.n_threads;
    ctx_params.n_threads_batch = settings.n_threads_batch; 
    
    // ÖNEMLİ: b7046'da bu fonksiyonun adı 'llama_init_from_model'
    ctx_ = llama_init_from_model(model_, ctx_params);
    if (!ctx_) {
        llama_model_free(model_);
        throw std::runtime_error("Failed to create llama_context");
    }

    model_loaded_ = true;
    spdlog::info("✅ LLM Engine initialized successfully.");
}

LLMEngine::~LLMEngine() {
    if (ctx_) llama_free(ctx_);
    if (model_) llama_model_free(model_);
    llama_backend_free();
    spdlog::info("LLM Engine shut down.");
}

bool LLMEngine::is_model_loaded() const { return model_loaded_.load(); }

void LLMEngine::generate_stream(
    const sentiric::llm::v1::LocalGenerateStreamRequest& request,
    const std::function<void(const std::string&)>& on_token_callback,
    const std::function<bool()>& should_stop_callback) {
    
    std::lock_guard<std::mutex> lock(engine_mutex_);

    // Context'i her seferinde temizle
    llama_memory_clear(llama_get_memory(ctx_), false);
    
    const auto* vocab = llama_model_get_vocab(model_);
    
    std::vector<llama_token> prompt_tokens(request.prompt().length());
    int n_tokens = llama_tokenize(vocab, request.prompt().c_str(), request.prompt().length(), prompt_tokens.data(), prompt_tokens.size(), true, false);
    if (n_tokens < 0) { throw std::runtime_error("Tokenization failed."); }
    prompt_tokens.resize(n_tokens);

    llama_batch batch = llama_batch_get_one(prompt_tokens.data(), n_tokens);
    if (llama_decode(ctx_, batch) != 0) {
        throw std::runtime_error("llama_decode failed on prompt");
    }

    const auto& params = request.params();
    const bool use_request_params = request.has_params();
    int32_t max_tokens = use_request_params && params.max_new_tokens() > 0 ? params.max_new_tokens() : settings_.default_max_tokens;
    float temperature = use_request_params ? params.temperature() : settings_.default_temperature;
    int32_t top_k = use_request_params ? params.top_k() : settings_.default_top_k;
    float top_p = use_request_params ? params.top_p() : settings_.default_top_p;
    float repeat_penalty = use_request_params ? params.repetition_penalty() : settings_.default_repeat_penalty;

    // Örnekleyici Zincirini Oluşturma (b7046'da var olan fonksiyonlarla)
    llama_sampler_chain_params sparams = llama_sampler_chain_default_params();
    llama_sampler* sampler_chain = llama_sampler_chain_init(sparams);
    
    llama_sampler_chain_add(sampler_chain, llama_sampler_init_penalties(llama_n_ctx(ctx_), repeat_penalty, 0.0f, 0.0f));
    llama_sampler_chain_add(sampler_chain, llama_sampler_init_top_k(top_k));
    llama_sampler_chain_add(sampler_chain, llama_sampler_init_top_p(top_p, 1));
    llama_sampler_chain_add(sampler_chain, llama_sampler_init_temp(temperature));
    llama_sampler_chain_add(sampler_chain, llama_sampler_init_greedy());

    int n_decoded = 0;
    while (llama_memory_seq_pos_max(llama_get_memory(ctx_), 0) < (int)llama_n_ctx(ctx_) && n_decoded < max_tokens) {
        if (should_stop_callback()) { break; }

        llama_token new_token_id = llama_sampler_sample(sampler_chain, ctx_, -1);
        llama_sampler_accept(sampler_chain, new_token_id);
        
        if (llama_vocab_is_eog(vocab, new_token_id)) { break; }
        
        char piece_buf[64] = {0};
        llama_token_to_piece(vocab, new_token_id, piece_buf, sizeof(piece_buf), 0, false);
        on_token_callback(std::string(piece_buf));
        
        llama_batch next_token_batch = llama_batch_get_one(&new_token_id, 1);
        if (llama_decode(ctx_, next_token_batch) != 0) {
             throw std::runtime_error("Failed to decode next token");
        }
        
        n_decoded++;
    }

    llama_sampler_free(sampler_chain);
}
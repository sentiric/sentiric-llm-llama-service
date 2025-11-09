#include "llm_engine.h"
#include "spdlog/spdlog.h"
#include <stdexcept>
#include <vector>
#include <string>
#include <algorithm>

// --- LLMEngine Sınıfı Implementasyonu (Modern API ile) ---

LLMEngine::LLMEngine(const Settings& settings) : settings_(settings) {
    spdlog::info("Initializing llama.cpp backend...");
    llama_backend_init();

    llama_model_params model_params = llama_model_default_params();
    model_ = llama_load_model_from_file(settings_.model_path.c_str(), model_params);
    if (!model_) {
        throw std::runtime_error("Failed to load model from " + settings_.model_path);
    }

    llama_context_params ctx_params = llama_context_default_params();
    ctx_params.n_ctx = settings_.context_size;
    ctx_params.n_threads = settings_.n_threads;
    ctx_params.n_threads_batch = settings_.n_threads_batch; // n_threads_batch kullanılıyor

    // Modern API: llama_init_from_model kullanılıyor
    ctx_ = llama_init_from_model(model_, ctx_params);
    if (!ctx_) {
        llama_free_model(model_);
        throw std::runtime_error("Failed to create context");
    }

    model_loaded_ = true;
    spdlog::info("✅ LLM Engine initialized successfully.");
}

LLMEngine::~LLMEngine() {
    if (ctx_) llama_free(ctx_);
    if (model_) llama_free_model(model_); // Eski API, ama hala çalışıyor. Moderni llama_model_free
    llama_backend_free();
}

bool LLMEngine::is_model_loaded() const {
    return model_loaded_.load();
}

const Settings& LLMEngine::get_settings() const {
    return settings_;
}

void LLMEngine::generate_stream(
    const sentiric::llm::v1::LocalGenerateStreamRequest& request,
    std::function<void(const std::string& token)> on_token_callback,
    std::function<bool()> should_stop_callback) {

    std::lock_guard<std::mutex> lock(generation_mutex_);

    // 1. Parametreleri ayarla
    const auto& grpc_params = request.params();
    int32_t max_tokens = grpc_params.has_max_new_tokens() ? grpc_params.max_new_tokens() : settings_.default_max_tokens;
    
    // 2. Tokenization
    // KESİN ÇÖZÜM: `llama_model_get_vocab` ve `llama_tokenize`
    const llama_vocab* vocab = llama_model_get_vocab(model_);
    std::vector<llama_token> prompt_tokens(request.prompt().size() + 1); // +1 for BOS
    int n_tokens = llama_tokenize(vocab, request.prompt().c_str(), request.prompt().length(), prompt_tokens.data(), prompt_tokens.size(), true, false);
    if (n_tokens < 0) {
        spdlog::error("Tokenization failed");
        return;
    }
    prompt_tokens.resize(n_tokens);

    // 3. Context'i hazırla
    const int n_ctx = llama_n_ctx(ctx_);
    if (n_tokens > n_ctx - 4) {
        spdlog::error("Prompt is too long for the context size.");
        return;
    }
    
    llama_kv_cache_clear(ctx_);

    // Prompt'u context'e işle
    llama_batch batch = llama_batch_get_one(prompt_tokens.data(), prompt_tokens.size(), 0, 0);
    if (llama_decode(ctx_, batch) != 0) {
        spdlog::error("llama_decode failed on prompt");
        return;
    }
    
    // 4. Sampler Zincirini Oluştur
    auto sparams = llama_sampler_chain_default_params();
    llama_sampler* sampler_chain = llama_sampler_chain_init(sparams);

    // KESİN ÇÖZÜM: Sampler'ları doğru imzalarla ekle (ctx_ ve nullptr olmadan)
    llama_sampler_chain_add(sampler_chain, llama_sampler_init_repetition_penalty(
        grpc_params.has_repetition_penalty() ? grpc_params.repetition_penalty() : settings_.default_repeat_penalty,
        settings_.repeat_last_n));
    llama_sampler_chain_add(sampler_chain, llama_sampler_init_top_k( 
        grpc_params.has_top_k() ? grpc_params.top_k() : settings_.default_top_k));
    llama_sampler_chain_add(sampler_chain, llama_sampler_init_top_p(
        grpc_params.has_top_p() ? grpc_params.top_p() : settings_.default_top_p, 1));
    llama_sampler_chain_add(sampler_chain, llama_sampler_init_temp(
        grpc_params.has_temperature() ? grpc_params.temperature() : settings_.default_temperature));

    int n_past = prompt_tokens.size();
    int n_remain = max_tokens;

    // 5. Generation Loop
    while (n_remain > 0) {
        if (should_stop_callback()) {
            spdlog::warn("Stream generation stopped by client cancellation.");
            break;
        }

        // Sampler zincirini kullanarak bir sonraki token'ı örnekle
        llama_token new_token_id = llama_sampler_sample(sampler_chain, ctx_, -1);

        // Örnekleyiciye bu token'ı kabul ettiğimizi bildir
        llama_sampler_accept(sampler_chain, ctx_, new_token_id);

        if (llama_vocab_is_eog(vocab, new_token_id)) {
            break;
        }

        char buf[128];
        int n = llama_token_to_piece(vocab, new_token_id, buf, sizeof(buf), 0, true);
        if (n < 0) {
            spdlog::error("Failed to convert token to piece");
            break;
        }
        on_token_callback(std::string(buf, n));

        // Sonraki token'ı decode etmek için context'i hazırla
        batch = llama_batch_get_one(&new_token_id, 1, n_past, 0);
        if (llama_decode(ctx_, batch) != 0) {
            spdlog::error("Failed to decode next token");
            break;
        }
        
        n_past++;
        n_remain--;
    }
    
    // Kaynakları temizle
    llama_sampler_free(sampler_chain);
}
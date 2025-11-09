#include "llm_engine.h"
#include "spdlog/spdlog.h"
#include <stdexcept>
#include <vector>
#include <string>
#include <algorithm>

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
    ctx_params.n_threads_batch = settings_.n_threads_batch;

    ctx_ = llama_new_context_with_model(model_, ctx_params);
    if (!ctx_) {
        llama_free_model(model_);
        throw std::runtime_error("Failed to create context");
    }

    model_loaded_ = true;
    spdlog::info("✅ LLM Engine initialized successfully.");
}

LLMEngine::~LLMEngine() {
    if (ctx_) llama_free(ctx_);
    if (model_) llama_free_model(model_);
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
    float temp = grpc_params.has_temperature() ? grpc_params.temperature() : settings_.default_temperature;
    int32_t top_k = grpc_params.has_top_k() ? grpc_params.top_k() : settings_.default_top_k;
    float top_p = grpc_params.has_top_p() ? grpc_params.top_p() : settings_.default_top_p;
    float repeat_penalty = grpc_params.has_repetition_penalty() ? grpc_params.repetition_penalty() : settings_.default_repeat_penalty;
    int32_t max_tokens = grpc_params.has_max_new_tokens() ? grpc_params.max_new_tokens() : settings_.default_max_tokens;

    // 2. Tokenization - KESİN ÇÖZÜM: Context ile tokenize et
    std::vector<llama_token> prompt_tokens(request.prompt().size());
    int n_tokens = llama_tokenize(ctx_, request.prompt().c_str(), request.prompt().length(), prompt_tokens.data(), prompt_tokens.size(), true, false);
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
    
    // KESİN ÇÖZÜM: KV Cache'i tüm sequence'leri silerek temizle
    llama_kv_cache_seq_rm(ctx_, 0, -1, -1);

    // Prompt'u context'e işle
    llama_batch batch = llama_batch_get_one(prompt_tokens.data(), n_tokens, 0, 0);
    if (llama_decode(ctx_, batch) != 0) {
        spdlog::error("llama_decode failed on prompt");
        return;
    }

    int n_past = n_tokens;
    int n_remain = max_tokens;

    std::vector<llama_token> generated_tokens = prompt_tokens;

    // 4. Generation Loop
    while (n_remain > 0) {
        if (should_stop_callback()) {
            spdlog::warn("Stream generation stopped by client cancellation.");
            break;
        }

        // KESİN ÇÖZÜM: Tek ve modern sampler fonksiyonunu kullan
        llama_token new_token_id = llama_sample_top_p_top_k(
            ctx_,
            llama_get_logits(ctx_),
            generated_tokens.data(),
            generated_tokens.size(),
            top_k,
            top_p,
            temp,
            repeat_penalty
        );

        if (new_token_id == llama_token_eos(model_)) {
            break;
        }

        std::string token_str = llama_token_to_piece(ctx_, new_token_id);
        on_token_callback(token_str);
        
        generated_tokens.push_back(new_token_id);

        // Sonraki token'ı decode etmek için context'i hazırla
        batch = llama_batch_get_one(&new_token_id, 1, n_past, 0);
        if (llama_decode(ctx_, batch) != 0) {
            spdlog::error("Failed to decode next token");
            break;
        }
        
        n_past++;
        n_remain--;
    }
}
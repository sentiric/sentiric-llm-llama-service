#include "llm_engine.h"
#include "spdlog/spdlog.h"
#include <stdexcept>
#include <vector>
#include <string>
#include <algorithm>

// --- LLMEngine Sınıfı Implementasyonu ---

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
    ctx_params.n_threads_batch = settings_.n_threads;

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
    int32_t repeat_last_n = settings_.repeat_last_n;

    // 2. Tokenization
    // Modern API: `llama_tokenize` artık `model` alır ve `vector` döndüren bir overload'u vardır.
    std::vector<llama_token> prompt_tokens = llama_tokenize(model_, request.prompt(), true);

    // 3. Context'i hazırla
    const int n_ctx = llama_n_ctx(ctx_);
    if ((int)prompt_tokens.size() > n_ctx - 4) {
        spdlog::error("Prompt is too long for the context size.");
        return;
    }
    
    // Modern API: KV Cache'i temizle
    llama_kv_cache_clear(ctx_);

    // Prompt'u context'e işle
    for (int i = 0; i < (int)prompt_tokens.size(); i++) {
        llama_decode(ctx_, llama_batch_get_one(&prompt_tokens[i], 1, i, 0));
    }
    
    int n_past = prompt_tokens.size();
    int n_remain = max_tokens;

    std::vector<llama_token> last_n_tokens;
    last_n_tokens.reserve(repeat_last_n);
    last_n_tokens.insert(last_n_tokens.end(), prompt_tokens.begin(), prompt_tokens.end());
    
    // 4. Generation Loop
    while (n_remain > 0) {
        if (should_stop_callback()) {
            spdlog::warn("Stream generation stopped by client cancellation.");
            break;
        }

        auto* logits = llama_get_logits(ctx_);
        auto n_vocab = llama_n_vocab(model_);

        std::vector<llama_token_data> candidates;
        candidates.reserve(n_vocab);
        for (llama_token token_id = 0; token_id < n_vocab; token_id++) {
            candidates.emplace_back(llama_token_data{token_id, logits[token_id], 0.0f});
        }
        llama_token_data_array candidates_p = {candidates.data(), candidates.size(), false};

        // Modern API Sampler'larını kullan
        llama_sample_repetition_penalties(ctx_, &candidates_p, last_n_tokens.data(), last_n_tokens.size(), repeat_penalty, 1.0f, 1.0f);
        llama_sample_top_k(ctx_, &candidates_p, top_k, 1);
        llama_sample_top_p(ctx_, &candidates_p, top_p, 1);
        llama_sample_temp(ctx_, &candidates_p, temp);
        llama_token new_token_id = llama_sample_token(ctx_, &candidates_p);

        if (new_token_id == llama_token_eos(model_)) {
            break;
        }

        std::string token_str = llama_token_to_piece(ctx_, new_token_id);
        on_token_callback(token_str);

        last_n_tokens.push_back(new_token_id);
        if (last_n_tokens.size() > (size_t)repeat_last_n) {
            last_n_tokens.erase(last_n_tokens.begin());
        }

        if (llama_decode(ctx_, llama_batch_get_one(&new_token_id, 1, n_past, 0)) != 0) {
            spdlog::error("Failed to decode next token");
            break;
        }
        
        n_past++;
        n_remain--;
    }
}
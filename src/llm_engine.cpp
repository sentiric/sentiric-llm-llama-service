#include "llm_engine.h"
#include "spdlog/spdlog.h"
#include <stdexcept>
#include <vector>
#include <string>
#include <algorithm> // for std::min

LLMEngine::LLMEngine(const Settings& settings) : settings_(settings) {
    spdlog::info("Initializing llama.cpp backend...");
    llama_backend_init();

    llama_model_params model_params = llama_model_default_params();
    model_ = llama_load_model_from_file(settings_.model_path.c_str(), model_params);
    if (!model_) throw std::runtime_error("Failed to load model from " + settings_.model_path);

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
    const sentiric::llm::v1::LocalGenerateRequest& request,
    std::function<void(const std::string& token)> on_token_callback,
    std::function<bool()> should_stop_callback) {

    std::lock_guard<std::mutex> lock(generation_mutex_);
    llama_kv_cache_clear(ctx_);

    // 1. Parametreleri ayarla
    const auto& params = request.params();
    float temp = params.has_temperature() ? params.temperature() : settings_.default_temperature;
    int32_t top_k = params.has_top_k() ? params.top_k() : settings_.default_top_k;
    float top_p = params.has_top_p() ? params.top_p() : settings_.default_top_p;
    float repeat_penalty = params.has_repetition_penalty() ? params.repetition_penalty() : settings_.default_repeat_penalty;
    int32_t max_tokens = params.has_max_new_tokens() ? params.max_new_tokens() : settings_.default_max_tokens;

    // 2. Tokenization
    std::vector<llama_token> tokens_list;
    tokens_list.resize(request.prompt().size() + 1);
    int n_tokens = llama_tokenize(model_, request.prompt().c_str(), request.prompt().length(), tokens_list.data(), tokens_list.size(), true, false);
    if (n_tokens < 0) {
        spdlog::error("Tokenization failed");
        return;
    }
    tokens_list.resize(n_tokens);

    // 3. Prompt'u Değerlendir
    int n_ctx = llama_n_ctx(ctx_);
    if (n_tokens >= n_ctx) {
        spdlog::error("Prompt is too long ({}) for context size ({})", n_tokens, n_ctx);
        return;
    }

    if (llama_decode(ctx_, llama_batch_get_one(tokens_list.data(), n_tokens, 0, 0))) {
        spdlog::error("Failed to eval initial prompt tokens");
        return;
    }

    // 4. Generation Loop
    int tokens_generated = 0;
    const llama_token eos_token = llama_token_eos(model_);
    std::vector<llama_token> last_n_tokens(n_ctx, 0);
    std::copy(tokens_list.begin(), tokens_list.end(), last_n_tokens.end() - n_tokens);

    while (tokens_generated < max_tokens) {
        if (should_stop_callback()) {
            spdlog::warn("Stream generation stopped by client cancellation.");
            break;
        }

        // Sonraki token'ı örnekle
        float* logits = llama_get_logits(ctx_);
        const int n_vocab = llama_n_vocab(model_);

        // GÜVENLİ YÖNTEM: Ham new/delete yerine std::vector kullan.
        // Bellek yönetimi otomatik ve exception-safe hale gelir.
        std::vector<llama_token_data> candidates_data;
        candidates_data.reserve(n_vocab);
        for (int i = 0; i < n_vocab; ++i) {
            candidates_data.push_back({ (llama_token)i, logits[i], 0.0f });
        }
        llama_token_data_array candidates = { candidates_data.data(), candidates_data.size(), false };

        llama_sample_repetition_penalty(ctx_, &candidates, last_n_tokens.data() + n_ctx - settings_.repeat_last_n, settings_.repeat_last_n, repeat_penalty);
        llama_sample_top_k(ctx_, &candidates, top_k, 1);
        llama_sample_top_p(ctx_, &candidates, top_p, 1);
        llama_sample_temp(ctx_, &candidates, temp);

        llama_token new_token = llama_sample_token(ctx_, &candidates);
        // delete[] çağrısına gerek kalmadı.

        if (new_token == eos_token) break;

        // Token'ı string'e çevir ve callback'i çağır
        const char* token_str = llama_token_to_str(model_, new_token);
        if (token_str) {
            on_token_callback(std::string(token_str));
        }

        // Yeni token'ı bağlama ekle
        if (llama_decode(ctx_, llama_batch_get_one(&new_token, 1, n_tokens, 0))) {
            spdlog::error("Failed to eval next token");
            break;
        }
        n_tokens++;
        tokens_generated++;
        last_n_tokens.erase(last_n_tokens.begin());
        last_n_tokens.push_back(new_token);
    }
}
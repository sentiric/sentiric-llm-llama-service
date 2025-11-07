#include "llm_engine.h"
#include "spdlog/spdlog.h"
#include <stdexcept>
#include <vector>
#include <string>
#include <mutex> // Mutex zaten .h'de ama tekrar olabilir

LLMEngine::LLMEngine(const Settings& settings) : settings_(settings) {
    spdlog::info("Initializing llama.cpp backend...");
    llama_backend_init();

    llama_model_params model_params = llama_model_default_params();
    model_ = llama_load_model_from_file(settings_.model_path.c_str(), model_params);
    if (!model_) throw std::runtime_error("Failed to load model");

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

void LLMEngine::generate_stream(
    const std::string& prompt,
    std::function<void(const std::string& token)> on_token_callback,
    float temperature, // Yeni parametre
    int top_k,         // Yeni parametre
    float top_p) {     // Yeni parametre

    // KRİTİK DÜZELTME: Kümeyi (context) korumak için Mutex Kilitlenmesi
    std::lock_guard<std::mutex> lock(generation_mutex_);

    // KV cache temizleme
    llama_kv_cache_clear(ctx_);
    
    // Tokenization
    std::vector<llama_token> tokens_list;
    tokens_list.resize(prompt.size() + 1);
    
    int n_tokens = llama_tokenize(model_, prompt.c_str(), tokens_list.data(), tokens_list.size(), true);
    if (n_tokens < 0) {
        spdlog::error("Tokenization failed");
        return;
    }
    tokens_list.resize(n_tokens);

    int n_ctx = llama_n_ctx(ctx_);
    if (n_tokens > n_ctx - 4) {
        tokens_list.resize(n_ctx - 4);
        n_tokens = n_ctx - 4;
    }
    
    // Eval için batch hazırla
    llama_batch batch = llama_batch_init(settings_.n_batch, 0, 1);
    for (int i = 0; i < n_tokens; ++i) {
        llama_batch_add(batch, tokens_list[i], i, { 0 }, false);
    }
    batch.logits[batch.n_tokens - 1] = true; // Sadece son token için logit hesapla

    if (llama_decode(ctx_, batch)) {
        spdlog::error("Failed to eval initial prompt tokens");
        return;
    }
    
    // Generation loop
    llama_token new_token = 0;
    const llama_token eos_token = llama_token_eos(model_);
    
    while (new_token != eos_token) {
        // Sonraki token için logitleri al
        float* logits = llama_get_logits_ith(ctx_, batch.n_tokens - 1);
        int n_vocab = llama_n_vocab(model_);

        // Gelişmiş Sampling (Top-k / Top-p) Uygulanması
        llama_token_data_array candidates = { logits, n_vocab, false };

        // Sıcaklık 0'a yakınsa (Greedy'ye yakınsa) kısıtla
        if (temperature <= 0.1f) {
            new_token = llama_sample_token_greedy(ctx_, &candidates);
        } else {
            // Temperature, Top-k, Top-p uygulaması
            llama_sample_top_k(ctx_, &candidates, top_k, 1);
            llama_sample_top_p(ctx_, &candidates, top_p, 1);
            
            // Sonraki token'ı örnekle
            new_token = llama_sample_token(ctx_, &candidates);
        }

        if (new_token == eos_token) {
            break;
        }

        // Token'ı string'e çevir ve callback'i çağır
        const char* token_str = llama_token_to_str(model_, new_token);
        if (token_str) {
            on_token_callback(std::string(token_str));
        }

        // Yeni token'ı bağlama (context) ekle
        llama_batch_clear(batch);
        llama_batch_add(batch, new_token, n_tokens, { 0 }, true);

        // Yeni token için bağlamı değerlendir
        if (llama_decode(ctx_, batch)) {
            spdlog::error("Failed to eval next token");
            break;
        }
        n_tokens++;
        
        // Context dolmasını engellemek için basit bir kontrol
        if (n_tokens >= n_ctx - 4) {
            spdlog::warn("Context window is full. Stopping generation.");
            break;
        }
    }
    llama_batch_free(batch); // Batch'i serbest bırakmayı unutma
}
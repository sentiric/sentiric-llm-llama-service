#include "llm_engine.h"
#include "spdlog/spdlog.h"
#include <stdexcept>
#include <vector>
#include <string>
#include <algorithm>
#include "model_manager.h"

// --- LlamaContextPool Implementasyonu ---
LlamaContextPool::LlamaContextPool(llama_model* model, const Settings& settings, size_t pool_size)
    : model_(model), settings_(settings) {
    if (!model) throw std::runtime_error("LlamaContextPool: model pointer is null.");
    for (size_t i = 0; i < pool_size; ++i) {
        llama_context_params ctx_params = llama_context_default_params();
        ctx_params.n_ctx = settings.context_size;
        ctx_params.n_threads = settings.n_threads;
        ctx_params.n_threads_batch = settings.n_threads_batch; 
        
        llama_context* ctx = llama_new_context_with_model(model_, ctx_params);
        if (ctx) {
            pool_.push(ctx);
        } else {
            while(!pool_.empty()) { llama_free(pool_.front()); pool_.pop(); }
            throw std::runtime_error("Failed to create llama_context #" + std::to_string(i + 1));
        }
    }
    if (pool_.empty()) throw std::runtime_error("LlamaContextPool initialized with zero contexts.");
    spdlog::info("LlamaContextPool initialized with {} contexts.", pool_.size());
}

LlamaContextPool::~LlamaContextPool() {
    while (!pool_.empty()) {
        llama_free(pool_.front());
        pool_.pop();
    }
}

llama_context* LlamaContextPool::acquire() {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this] { return !pool_.empty(); });
    llama_context* ctx = pool_.front();
    pool_.pop();
    return ctx;
}

void LlamaContextPool::release(llama_context* ctx) {
    if (ctx) {
        std::lock_guard<std::mutex> lock(mutex_);
        // Not: b7046'da güvenli bir public KV cache temizleme yöntemi yok.
        // Bu yüzden context'i serbest bırakıp yeniden oluşturmak en güvenli yoldur.
        // Performans için bu bir sonraki adımda iyileştirilebilir.
        // Şimdilik, sadece geri koyuyoruz ve state'in bir sonraki kullanımda
        // üzerine yazılacağını varsayıyoruz.
        pool_.push(ctx);
        cv_.notify_one();
    }
}

// --- LLMEngine Implementasyonu ---
LLMEngine::LLMEngine(Settings& settings) : settings_(settings) {
    spdlog::info("Initializing LLM Engine...");
    settings_.model_path = ModelManager::ensure_model_is_ready(settings_);
    
    spdlog::info("Initializing llama.cpp backend...");
    llama_backend_init();

    llama_model_params model_params = llama_model_default_params();
    model_params.n_gpu_layers = settings.n_gpu_layers;
    
    model_ = llama_load_model_from_file(settings.model_path.c_str(), model_params);
    if (!model_) throw std::runtime_error("Model loading failed from path: " + settings.model_path);

    try {
        size_t pool_size = settings.n_threads > 0 ? settings.n_threads : 1;
        context_pool_ = std::make_unique<LlamaContextPool>(model_, settings, pool_size);
        model_loaded_ = true;
        spdlog::info("✅ LLM Engine initialized successfully.");
    } catch (const std::exception& e) {
        llama_free_model(model_); model_ = nullptr;
        throw;
    }
}

LLMEngine::~LLMEngine() {
    context_pool_.reset();
    if (model_) llama_free_model(model_);
    llama_backend_free();
    spdlog::info("LLM Engine shut down.");
}

bool LLMEngine::is_model_loaded() const { return model_loaded_.load(); }

void LLMEngine::generate_stream(
    const sentiric::llm::v1::LocalGenerateStreamRequest& request,
    const std::function<void(const std::string&)>& on_token_callback,
    const std::function<bool()>& should_stop_callback) {
    
    llama_context* ctx = context_pool_->acquire();
    if (!ctx) throw std::runtime_error("Failed to acquire a llama_context from the pool.");
    struct ContextReleaser {
        LlamaContextPool* pool; llama_context* ctx;
        ~ContextReleaser() { if (pool && ctx) pool->release(ctx); }
    } releaser{context_pool_.get(), ctx};

    // Her kullanımdan önce context'in KV cache'ini sıfırla.
    llama_kv_cache_clear(ctx);

    std::vector<llama_token> prompt_tokens(request.prompt().length());
    int n_tokens = llama_tokenize(model_, request.prompt().c_str(), request.prompt().length(), prompt_tokens.data(), prompt_tokens.size(), true);
    if (n_tokens < 0) { throw std::runtime_error("Tokenization failed."); }
    prompt_tokens.resize(n_tokens);

    // Prompt'u işle
    if (llama_eval(ctx, prompt_tokens.data(), n_tokens, 0) != 0) {
        throw std::runtime_error("Failed to eval prompt");
    }

    const auto& params = request.params();
    const bool use_request_params = request.has_params();
    int32_t max_tokens = use_request_params && params.max_new_tokens() > 0 ? params.max_new_tokens() : settings_.default_max_tokens;
    float temperature = use_request_params ? params.temperature() : settings_.default_temperature;
    int32_t top_k = use_request_params ? params.top_k() : settings_.default_top_k;
    float top_p = use_request_params ? params.top_p() : settings_.default_top_p;
    
    int n_decoded = 0;
    while (llama_get_kv_cache_token_count(ctx) < (int32_t)llama_n_ctx(ctx) && n_decoded < max_tokens) {
        if (should_stop_callback()) { break; }

        auto logits = llama_get_logits(ctx);
        auto n_vocab = llama_n_vocab(model_);

        std::vector<llama_token_data> candidates;
        candidates.reserve(n_vocab);
        for (llama_token token_id = 0; token_id < n_vocab; token_id++) {
            candidates.emplace_back(llama_token_data{token_id, logits[token_id], 0.0f});
        }
        llama_token_data_array candidates_p = { candidates.data(), candidates.size(), false };

        // SADECE TEMEL API FONKSİYONLARI KULLANILIYOR
        llama_sample_top_k(ctx, &candidates_p, top_k, 1);
        llama_sample_tail_free(ctx, &candidates_p, 1.0f, 1);
        llama_sample_typical(ctx, &candidates_p, 1.0f, 1);
        llama_sample_top_p(ctx, &candidates_p, top_p, 1);
        llama_sample_temperature(ctx, &candidates_p, temperature);
        llama_token new_token_id = llama_sample_token(ctx, &candidates_p);

        if (new_token_id == llama_token_eos(model_)) { break; }
        
        char piece_buf[8] = {0};
        llama_token_to_str(ctx, new_token_id, piece_buf, sizeof(piece_buf));
        on_token_callback(std::string(piece_buf));
        
        if (llama_eval(ctx, &new_token_id, 1, llama_get_kv_cache_token_count(ctx)) != 0) {
             throw std::runtime_error("Failed to eval next token");
        }
        
        n_decoded++;
    }
}
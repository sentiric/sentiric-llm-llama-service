#include "llm_engine.h"
#include "spdlog/spdlog.h"
#include <stdexcept>
#include <vector>
#include <string>
#include <algorithm>
#include "model_manager.h"
#include "common.h"
#include "llama.h"

// --- ContextGuard Implementasyonu (DEĞİŞİKLİK YOK) ---
ContextGuard::ContextGuard(LlamaContextPool* pool, llama_context* ctx) : pool_(pool), ctx_(ctx) {
    if (!ctx_) throw std::runtime_error("Acquired null llama_context.");
}
ContextGuard::~ContextGuard() {
        // KB Referansı: 2.4. KV Cache Yönetimi (DOĞRU KULLANIM)
        llama_memory_seq_rm(llama_get_memory(ctx_), -1, -1, -1);
        pool_->release(ctx_);
}
ContextGuard::ContextGuard(ContextGuard&& other) noexcept : pool_(other.pool_), ctx_(other.ctx_) {
    other.pool_ = nullptr;
    other.ctx_ = nullptr;
}
ContextGuard& ContextGuard::operator=(ContextGuard&& other) noexcept {
    if (this != &other) {
        if (ctx_ && pool_) {
            // KB Referansı: 2.4. KV Cache Yönetimi (DOĞRU KULLANIM)
            llama_memory_seq_rm(llama_get_memory(ctx_), -1, -1, -1);
            pool_->release(ctx_);
        }
        pool_ = other.pool_;
        ctx_ = other.ctx_;
        other.pool_ = nullptr;
        other.ctx_ = nullptr;
    }
    return *this;
}

// --- LlamaContextPool Implementasyonu (DEĞİŞİKLİK YOK) ---
LlamaContextPool::LlamaContextPool(const Settings& settings, llama_model* model) : model_(model), settings_(settings) {
    max_size_ = settings.n_threads;
    if (max_size_ == 0) { max_size_ = 1; }
    spdlog::info("Context Pool: Initializing {} llama_context instances...", max_size_);
    initialize_contexts();
}
LlamaContextPool::~LlamaContextPool() {
    std::lock_guard<std::mutex> lock(mutex_);
    while (!pool_.empty()) {
        llama_context* ctx = pool_.front();
        pool_.pop();
        llama_free(ctx);
    }
    spdlog::info("Context Pool: Destroyed {} llama_context instances.", max_size_);
}
void LlamaContextPool::initialize_contexts() {
    if (!model_) return;
    for (size_t i = 0; i < max_size_; ++i) {
        llama_context_params ctx_params = llama_context_default_params();
        ctx_params.n_ctx = settings_.context_size;
        ctx_params.n_threads = 1;
        ctx_params.n_threads_batch = 1;
        ctx_params.kv_unified = true;
        llama_context* ctx = llama_init_from_model(model_, ctx_params);
        if (!ctx) throw std::runtime_error("Failed to create llama_context for pool instance.");
        pool_.push(ctx);
        spdlog::debug("Context Pool: Instance {}/{} created.", i + 1, max_size_);
    }
}
ContextGuard LlamaContextPool::acquire() {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this]{ return !pool_.empty(); });
    llama_context* ctx = pool_.front();
    pool_.pop();
    return ContextGuard(this, ctx);
}
void LlamaContextPool::release(llama_context* ctx) {
    if (!ctx) return;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pool_.push(ctx);
    }
    cv_.notify_one();
}

// --- LLMEngine Implementasyonu ---

LLMEngine::LLMEngine(const Settings& settings) : settings_(settings) {
    spdlog::info("Initializing LLM Engine...");
    llama_backend_init();
    llama_model_params model_params = llama_model_default_params();
    model_params.n_gpu_layers = settings.n_gpu_layers;
    model_ = llama_model_load_from_file(settings.model_path.c_str(), model_params);
    if (!model_) throw std::runtime_error("Model loading failed from path: " + settings.model_path);
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
    const sentiric::llm::v1::LocalGenerateStreamRequest& request,
    const std::function<void(const std::string&)>& on_token_callback,
    const std::function<bool()>& should_stop_callback) {
    
    ContextGuard guard = context_pool_->acquire();
    llama_context* ctx = guard.get();

    const auto* vocab = llama_model_get_vocab(model_);
    
    std::vector<llama_token> prompt_tokens;
    prompt_tokens.resize(request.prompt().length() + 16);
    int n_tokens = llama_tokenize(vocab, request.prompt().c_str(), request.prompt().length(), prompt_tokens.data(), prompt_tokens.size(), true, true);
    if (n_tokens < 0) { throw std::runtime_error("Tokenization failed."); }
    prompt_tokens.resize(n_tokens);

    llama_batch batch = llama_batch_init(n_tokens, 0, 1);
    for(int i = 0; i < n_tokens; ++i) {
        common_batch_add(batch, prompt_tokens[i], i, {0}, false);
    }
    batch.logits[batch.n_tokens - 1] = true;

    if (llama_decode(ctx, batch) != 0) {
        llama_batch_free(batch);
        throw std::runtime_error("llama_decode failed on prompt");
    }

    const auto& params = request.params();
    const bool use_request_params = request.has_params();
    
    int32_t max_tokens = use_request_params && params.max_new_tokens() > 0 ? params.max_new_tokens() : settings_.default_max_tokens;
    float temperature = use_request_params && params.has_temperature() ? params.temperature() : settings_.default_temperature;
    int32_t top_k = use_request_params && params.has_top_k() ? params.top_k() : settings_.default_top_k;
    float top_p = use_request_params && params.has_top_p() ? params.top_p() : settings_.default_top_p;
    float repeat_penalty = use_request_params && params.has_repetition_penalty() ? params.repetition_penalty() : settings_.default_repeat_penalty;

    llama_sampler_chain_params sparams = llama_sampler_chain_default_params();
    llama_sampler* sampler_chain = llama_sampler_chain_init(sparams);
    
    // NİHAİ DÜZELTME: `llama_sampler_init_penalties` için doğru imza.
    llama_sampler_chain_add(sampler_chain, llama_sampler_init_penalties(llama_n_ctx(ctx), repeat_penalty, 0.0f, 0.0f));
    
    llama_sampler_chain_add(sampler_chain, llama_sampler_init_top_k(top_k));
    llama_sampler_chain_add(sampler_chain, llama_sampler_init_top_p(top_p, 1));
    llama_sampler_chain_add(sampler_chain, llama_sampler_init_temp(temperature));
    llama_sampler_chain_add(sampler_chain, llama_sampler_init_dist(settings_.default_top_k));

    int n_decoded = 0;
    llama_pos n_past = batch.n_tokens;
    
    while (n_past < (int)llama_n_ctx(ctx) && n_decoded < max_tokens) {
        if (should_stop_callback()) { break; }

        llama_token new_token_id = llama_sampler_sample(sampler_chain, ctx, -1);
        llama_sampler_accept(sampler_chain, new_token_id);
        
        if (llama_vocab_is_eog(vocab, new_token_id)) { break; }
        
        char piece_buf[64] = {0};
        int n_piece = llama_token_to_piece(vocab, new_token_id, piece_buf, sizeof(piece_buf), 0, false);
        if (n_piece > 0) {
            on_token_callback(std::string(piece_buf, n_piece));
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

    llama_batch_free(batch);
    llama_sampler_free(sampler_chain);
}
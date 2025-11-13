#include "llm_engine.h"
#include "spdlog/spdlog.h"
#include <stdexcept>
#include <vector>
#include <string>
#include <algorithm>

// --- LlamaContextPool Implementasyonu ---

LlamaContextPool::LlamaContextPool(llama_model* model, const Settings& settings, size_t pool_size)
    : model_(model), settings_(settings) {
    if (!model) throw std::runtime_error("LlamaContextPool: model pointer is null.");
    for (size_t i = 0; i < pool_size; ++i) {
        llama_context_params ctx_params = llama_context_default_params();
        ctx_params.n_ctx = settings.context_size;
        ctx_params.n_threads = settings.n_threads;
        ctx_params.n_threads_batch = settings.n_threads_batch;
        ctx_params.n_batch = settings.n_batch;
        ctx_params.n_ubatch = settings.n_ubatch;

        llama_context* ctx = llama_init_from_model(model_, ctx_params);
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
        llama_kv_cache_clear(ctx);
        pool_.push(ctx);
        cv_.notify_one();
    }
}

// --- LLMEngine Implementasyonu ---

LLMEngine::LLMEngine(const Settings& settings) : settings_(settings) {
    spdlog::info("Initializing llama.cpp backend...");
    llama_backend_init();
    llama_numa_init(settings.numa_strategy);

    llama_model_params model_params = llama_model_default_params();
    model_params.n_gpu_layers = settings.n_gpu_layers;
    
    model_ = llama_model_load_from_file(settings.model_path.c_str(), model_params);
    if (!model_) throw std::runtime_error("Model loading failed from path: " + settings.model_path);

    try {
        size_t pool_size = settings.n_threads > 0 ? settings.n_threads : 1;
        context_pool_ = std::make_unique<LlamaContextPool>(model_, settings, pool_size);
        model_loaded_ = true;
        spdlog::info("✅ LLM Engine initialized successfully.");
    } catch (const std::exception& e) {
        llama_model_free(model_); model_ = nullptr;
        throw;
    }
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
    
    llama_context* ctx = context_pool_->acquire();
    if (!ctx) throw std::runtime_error("Failed to acquire a llama_context from the pool.");
    struct ContextReleaser {
        LlamaContextPool* pool; llama_context* ctx;
        ~ContextReleaser() { if (pool && ctx) pool->release(ctx); }
    } releaser{context_pool_.get(), ctx};

    const llama_vocab* vocab = llama_model_get_vocab(model_);

    std::vector<llama_token> prompt_tokens;
    prompt_tokens.resize(request.prompt().length() + 1);
    int n_tokens = llama_tokenize(vocab, request.prompt().c_str(), request.prompt().length(), prompt_tokens.data(), prompt_tokens.size(), true, false);
    if (n_tokens < 0) throw std::runtime_error("Prompt tokenization failed.");
    prompt_tokens.resize(n_tokens);
    
    llama_batch batch = llama_batch_get_one(prompt_tokens.data(), n_tokens);
    if (llama_decode(ctx, batch)) {
        throw std::runtime_error("llama_decode failed on prompt");
    }

    const auto& params = request.params();
    int32_t max_tokens = (params.has_max_new_tokens() && params.max_new_tokens() > 0) ? params.max_new_tokens() : settings_.default_max_tokens;
    
    llama_sampler* smpl = llama_sampler_chain_init(llama_sampler_chain_default_params());
    struct SamplerReleaser {
        llama_sampler* s; ~SamplerReleaser() { if(s) llama_sampler_free(s); }
    } sampler_releaser{smpl};
    
    llama_sampler_chain_add(smpl, llama_sampler_init_penalties(64, params.has_repetition_penalty() ? params.repetition_penalty() : settings_.default_repeat_penalty, 0.0f, 0.0f));
    llama_sampler_chain_add(smpl, llama_sampler_init_top_k(params.has_top_k() ? params.top_k() : settings_.default_top_k));
    llama_sampler_chain_add(smpl, llama_sampler_init_top_p(params.has_top_p() ? params.top_p() : settings_.default_top_p, 1));
    llama_sampler_chain_add(smpl, llama_sampler_init_temp(params.has_temperature() ? params.temperature() : settings_.default_temperature));
    llama_sampler_chain_add(smpl, llama_sampler_init_greedy());

    int n_decode = 0;
    while (llama_memory_seq_pos_max(llama_get_memory(ctx), 0) + 1 < (int32_t)llama_n_ctx(ctx) && n_decode < max_tokens) {
        if (should_stop_callback()) {
            spdlog::warn("Stream generation stopped by client cancellation.");
            break;
        }

        llama_token new_token_id = llama_sampler_sample(smpl, ctx, -1);
        llama_sampler_accept(smpl, new_token_id);

        if (new_token_id == llama_vocab_eos(vocab)) break;
        
        char piece_buf[64];
        int n_chars = llama_token_to_piece(vocab, new_token_id, piece_buf, sizeof(piece_buf), 0, false);
        if (n_chars < 0) throw std::runtime_error("llama_token_to_piece failed");
        on_token_callback(std::string(piece_buf, n_chars));

        llama_batch next_token_batch = llama_batch_get_one(&new_token_id, 1);
        if (llama_decode(ctx, next_token_batch)) {
             throw std::runtime_error("Failed to decode next token");
        }
        n_decode++;
    }
}
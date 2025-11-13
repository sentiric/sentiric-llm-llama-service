#include "llm_engine.h"
#include "spdlog/spdlog.h"
#include <stdexcept>
#include <vector>
#include <string>
#include <algorithm>

// --- LlamaContextPool Implementasyonu ---

LlamaContextPool::LlamaContextPool(llama_model* model, const Settings& settings, size_t pool_size)
    : model_(model), settings_(settings) {
    if (!model) {
        throw std::runtime_error("LlamaContextPool: Model pointer is null.");
    }
    for (size_t i = 0; i < pool_size; ++i) {
        llama_context_params ctx_params = llama_context_default_params();
        ctx_params.n_ctx = settings_.context_size;
        ctx_params.n_threads = settings_.n_threads;
        ctx_params.n_threads_batch = settings_.n_threads;
        
        llama_context* ctx = llama_new_context_with_model(model_, ctx_params);
        if (ctx) {
            pool_.push(ctx);
        } else {
            while(!pool_.empty()) {
                llama_free(pool_.front());
                pool_.pop();
            }
            throw std::runtime_error("Failed to create llama_context #" + std::to_string(i + 1));
        }
    }
    if (pool_.empty()) {
        throw std::runtime_error("LlamaContextPool initialized with zero contexts.");
    }
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
        llama_kv_cache_seq_rm(ctx, -1, -1, -1);
        pool_.push(ctx);
        cv_.notify_one();
    }
}


// --- LLMEngine Implementasyonu ---

LLMEngine::LLMEngine(const Settings& settings) : settings_(settings) {
    spdlog::info("Initializing llama.cpp backend...");

    llama_backend_init();
    llama_numa_init(GGML_NUMA_STRATEGY_DISTRIBUTE);

    llama_model_params model_params = llama_model_default_params();
    model_params.n_gpu_layers = settings_.n_gpu_layers;
    
    model_ = llama_load_model_from_file(settings_.model_path.c_str(), model_params);
    
    if (!model_) {
        model_loaded_ = false;
        throw std::runtime_error("Model loading failed from path: " + settings_.model_path);
    }

    try {
        size_t pool_size = settings_.n_threads > 0 ? settings_.n_threads : 1;
        context_pool_ = std::make_unique<LlamaContextPool>(model_, settings_, pool_size);
        model_loaded_ = true;
        spdlog::info("✅ LLM Engine initialized successfully.");
    } catch (const std::exception& e) {
        model_loaded_ = false;
        llama_free_model(model_);
        model_ = nullptr;
        throw;
    }
}

LLMEngine::~LLMEngine() {
    context_pool_.reset();
    if (model_) llama_free_model(model_);
    llama_backend_free();
    spdlog::info("LLM Engine shut down.");
}

bool LLMEngine::is_model_loaded() const {
    return model_loaded_.load();
}

void LLMEngine::generate_stream(
    const sentiric::llm::v1::LocalGenerateStreamRequest& request,
    const std::function<void(const std::string&)>& on_token_callback,
    const std::function<bool()>& should_stop_callback) {
    
    llama_context* ctx = context_pool_->acquire();
    if (!ctx) {
        throw std::runtime_error("Failed to acquire a llama_context from the pool.");
    }

    struct ContextReleaser {
        LlamaContextPool* pool;
        llama_context* ctx;
        ~ContextReleaser() { if (pool && ctx) pool->release(ctx); }
    } releaser{context_pool_.get(), ctx};

    std::vector<llama_token> prompt_tokens;
    prompt_tokens.resize(settings_.context_size);
    int n_tokens = llama_tokenize(model_, request.prompt().c_str(), request.prompt().length(), prompt_tokens.data(), prompt_tokens.size(), true, false);
    if (n_tokens < 0) {
        throw std::runtime_error("Prompt is too long for tokenization buffer.");
    }
    prompt_tokens.resize(n_tokens);

    if (llama_decode(ctx, llama_batch_get_one(prompt_tokens.data(), n_tokens, 0, 0))) {
        throw std::runtime_error("llama_decode failed on prompt");
    }

    const auto& params = request.params();
    int32_t max_tokens = settings_.default_max_tokens;
    if (request.has_params() && request.params().has_max_new_tokens() && request.params().max_new_tokens() > 0) {
        max_tokens = request.params().max_new_tokens();
    }
    
    llama_token_data_array candidates;
    candidates.data = new llama_token_data[llama_n_vocab(model_)];
    candidates.size = llama_n_vocab(model_);
    candidates.sorted = false;
    
    struct CandidatesCleaner {
        llama_token_data* data;
        ~CandidatesCleaner() { delete[] data; }
    } cleaner{candidates.data};

    int n_decode = 0;
    while (llama_get_kv_cache_token_count(ctx) < settings_.context_size && n_decode < max_tokens) {
        if (should_stop_callback()) {
            spdlog::warn("Stream generation stopped by client cancellation.");
            break;
        }

        auto* logits = llama_get_logits_ith(ctx, llama_get_kv_cache_token_count(ctx) - 1);
        
        for (llama_token token_id = 0; token_id < (llama_token)candidates.size; ++token_id) {
            candidates.data[token_id].id = token_id;
            candidates.data[token_id].logit = logits[token_id];
            candidates.data[token_id].p = 0.0f;
        }

        llama_sample_repetition_penalties(ctx, &candidates, prompt_tokens.data(), prompt_tokens.size(), params.has_repetition_penalty() ? params.repetition_penalty() : settings_.default_repeat_penalty, 64, 1.0f);
        llama_sample_top_k(ctx, &candidates, params.has_top_k() ? params.top_k() : settings_.default_top_k, 1);
        llama_sample_top_p(ctx, &candidates, params.has_top_p() ? params.top_p() : settings_.default_top_p, 1);
        llama_sample_temp(ctx, &candidates, params.has_temperature() ? params.temperature() : settings_.default_temperature);
        
        llama_token new_token_id = llama_sample_token_greedy(ctx, &candidates);

        if (new_token_id == llama_token_eos(model_)) {
            break;
        }
        
        on_token_callback(llama_token_to_piece(ctx, new_token_id));
        prompt_tokens.push_back(new_token_id);

        if (llama_decode(ctx, llama_batch_get_one(&new_token_id, 1, llama_get_kv_cache_token_count(ctx), 0))) {
             throw std::runtime_error("Failed to decode next token");
        }
        n_decode++;
    }
}
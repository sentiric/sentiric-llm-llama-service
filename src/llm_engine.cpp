// src/llm_engine.cpp
#include "llm_engine.h"
#include "spdlog/spdlog.h"
#include <stdexcept>
#include <vector>
#include <string>
#include <algorithm>
// EKLENDİ: Dinamik örnekleyici parametreleri için
#include "llama_sampling.h"

// --- LlamaContextPool Implementasyonu (Değişiklik yok) ---

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
        
        llama_context* ctx = llama_init_from_model(model_, ctx_params);
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
        llama_memory_seq_rm(llama_get_memory(ctx), -1, -1, -1);
        pool_.push(ctx);
        cv_.notify_one();
    }
}


// --- LLMEngine Implementasyonu (Değişiklikler burada) ---

LLMEngine::LLMEngine(const Settings& settings) : settings_(settings) {
    spdlog::info("Initializing llama.cpp backend...");

    llama_backend_init();
    llama_numa_init(GGML_NUMA_STRATEGY_DISTRIBUTE);

    llama_model_params model_params = llama_model_default_params();
    model_params.n_gpu_layers = settings_.n_gpu_layers;
    
    model_ = llama_model_load_from_file(settings_.model_path.c_str(), model_params);
    
    if (!model_) {
        model_loaded_ = false;
        throw std::runtime_error("Model loading failed from path: " + settings_.model_path);
    }
    
    vocab_ = llama_model_get_vocab(model_);
    // DEĞİŞTİRİLDİ: Global sampler artık oluşturulmuyor, her istekte dinamik olarak yaratılacak.
    // sampler_ = llama_sampler_init_greedy(); // BU SATIR SİLİNDİ
    if (!vocab_) {
        llama_model_free(model_);
        throw std::runtime_error("Failed to initialize vocab.");
    }

    try {
        size_t pool_size = settings_.n_threads > 0 ? settings_.n_threads : 1;
        context_pool_ = std::make_unique<LlamaContextPool>(model_, settings_, pool_size);
        model_loaded_ = true;
        spdlog::info("✅ LLM Engine initialized successfully.");
    } catch (const std::exception& e) {
        model_loaded_ = false;
        llama_model_free(model_);
        model_ = nullptr;
        vocab_ = nullptr;
        throw;
    }
}

LLMEngine::~LLMEngine() {
    context_pool_.reset();
    if (model_) llama_model_free(model_);
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

    // RAII sarmalayıcısı ile context'in otomatik iade edilmesini sağla
    struct ContextReleaser {
        LlamaContextPool* pool;
        llama_context* ctx;
        ~ContextReleaser() { if (pool && ctx) pool->release(ctx); }
    } releaser{context_pool_.get(), ctx};

    // --- EKLENDİ: Dinamik Örnekleyici (Sampler) Yapılandırması ---
    llama_sampler_params sparams;
    const auto& params = request.params();

    sparams.temp = params.has_temperature() ? params.temperature() : settings_.default_temperature;
    sparams.top_k = params.has_top_k() ? params.top_k() : settings_.default_top_k;
    sparams.top_p = params.has_top_p() ? params.top_p() : settings_.default_top_p;
    sparams.penalty_repeat = params.has_repetition_penalty() ? params.repetition_penalty() : settings_.default_repeat_penalty;
    // Diğer parametreler de benzer şekilde eklenebilir.

    llama_sampler* sampler = llama_sampler_init_chain_default_with_params(&sparams);
    if (!sampler) {
        throw std::runtime_error("Failed to initialize dynamic sampler chain.");
    }
    
    // RAII sarmalayıcısı ile sampler'ın otomatik temizlenmesini sağla
    struct SamplerReleaser {
        llama_sampler* s;
        ~SamplerReleaser() { if (s) llama_sampler_free(s); }
    } sampler_releaser{sampler};
    // --- Değişiklik Sonu ---

    std::vector<llama_token> prompt_tokens(request.prompt().size());
    int n_tokens = llama_tokenize(vocab_, request.prompt().c_str(), request.prompt().size(), prompt_tokens.data(), prompt_tokens.size(), true, false);
    if (n_tokens < 0) {
        throw std::runtime_error("Prompt tokenization failed.");
    }
    prompt_tokens.resize(n_tokens);

    uint32_t n_ctx = llama_n_ctx(ctx);
    if (prompt_tokens.size() > n_ctx - 4) {
         throw std::runtime_error("Prompt is too long.");
    }

    llama_batch batch = llama_batch_get_one(prompt_tokens.data(), prompt_tokens.size());

    if (llama_decode(ctx, batch) != 0) {
        throw std::runtime_error("llama_decode failed on prompt");
    }

    int32_t max_tokens = settings_.default_max_tokens;
    if (request.has_params() && request.params().has_max_new_tokens()) {
        max_tokens = request.params().max_new_tokens();
    }
    
    int n_decode = 0;
    
    while (n_decode < max_tokens) {
        if (should_stop_callback()) {
            spdlog::warn("Stream generation stopped by client cancellation.");
            break;
        }

        // DEĞİŞTİRİLDİ: Global `sampler_` yerine isteğe özel `sampler` kullanılıyor.
        llama_token new_token_id = llama_sampler_sample(sampler, ctx, -1);
        llama_sampler_accept(sampler, new_token_id);

        if (llama_vocab_is_eog(vocab_, new_token_id)) {
            break;
        }
        
        char piece[128];
        int n_chars = llama_token_to_piece(vocab_, new_token_id, piece, sizeof(piece), 0, false);
        if (n_chars < 0) {
            throw std::runtime_error("llama_token_to_piece failed.");
        }
        on_token_callback(std::string(piece, n_chars));

        batch = llama_batch_get_one(&new_token_id, 1);

        n_decode += 1;

        if (llama_decode(ctx, batch) != 0) {
            throw std::runtime_error("Failed to decode next token");
        }
    }
}
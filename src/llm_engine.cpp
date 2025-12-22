#include "llm_engine.h"
#include "spdlog/spdlog.h"
#include "model_manager.h"
#include "common.h"
#include <stdexcept>
#include <time.h>
#include <algorithm>
#include <vector>
#include <thread>
#include <future>
#include <filesystem>
#include <regex>
#include <optional>

// --- RAII HELPERS ---

struct LlamaBatchScope {
    llama_batch batch;
    LlamaBatchScope(int32_t n_tokens, int32_t embd, int32_t n_seq_max) 
        : batch(llama_batch_init(n_tokens, embd, n_seq_max)) {}
    ~LlamaBatchScope() { if (batch.token) llama_batch_free(batch); }
    void clear() { batch.n_tokens = 0; }
};

struct LlamaSamplerGuard {
    llama_sampler* sampler;
    LlamaSamplerGuard(llama_sampler_chain_params params) 
        : sampler(llama_sampler_chain_init(params)) {}
    ~LlamaSamplerGuard() { if (sampler) llama_sampler_free(sampler); }
};

// --- CONSTRUCTOR & DESTRUCTOR ---

LLMEngine::LLMEngine(Settings& settings, prometheus::Gauge& active_contexts_gauge)
    : settings_(settings), active_contexts_gauge_(active_contexts_gauge) {
    
    spdlog::info("🚀 Initializing LLM Engine...");
    
    formatter_ = create_formatter(settings_.model_id);

    if (!reload_model(settings_.profile_name)) {
        throw std::runtime_error("Critical: Initial model load failed.");
    }

    size_t batch_size = settings_.enable_dynamic_batching ? settings_.max_batch_size : 1;
    batcher_ = std::make_unique<DynamicBatcher>(batch_size, std::chrono::milliseconds(settings_.batch_timeout_ms));
    batcher_->batch_processing_callback = [this](std::vector<std::shared_ptr<BatchedRequest>>& batch) {
        this->process_batch(batch);
    };
}

LLMEngine::~LLMEngine() {
    if (batcher_) batcher_->stop();
    {
        std::unique_lock<std::shared_mutex> lock(model_mutex_);
        clear_adapter_cache(); 
        context_pool_.reset();
        if (model_) llama_model_free(model_);
        llama_backend_free();
    }
}

// --- MODEL MANAGEMENT ---

bool LLMEngine::reload_model(const std::string& profile_name) {
    spdlog::info("🔄 Profile switch requested: {}", profile_name);

    Settings temp_settings = settings_; 
    if (!apply_profile(temp_settings, profile_name)) {
        spdlog::error("❌ Reload aborted: Profile '{}' invalid.", profile_name);
        return false;
    }

    try {
        spdlog::info("⬇️ checking/downloading model in background...");
        temp_settings.model_path = ModelManager::ensure_model_is_ready(temp_settings);
    } catch (const std::exception& e) {
        spdlog::error("❌ Background download failed: {}", e.what());
        return false;
    }

    settings_ = temp_settings;
    formatter_ = create_formatter(settings_.model_id);

    return internal_reload_model();
}

bool LLMEngine::update_hardware_config(int gpu_layers, int context_size, bool kv_offload) {
    spdlog::info("⚙️ Hardware Reconfiguration: GPU={} Layers, Context={}, KV_Offload={}", 
                 gpu_layers, context_size, kv_offload);
    
    settings_.n_gpu_layers = gpu_layers;
    settings_.context_size = context_size;
    settings_.kv_offload = kv_offload;
    
    return internal_reload_model();
}

bool LLMEngine::internal_reload_model() {
    std::unique_lock<std::shared_mutex> lock(model_mutex_);
    spdlog::info("🔒 System locked for model update...");
    
    model_loaded_ = false;

    try {
        clear_adapter_cache(); 
        context_pool_.reset(); 
        if (model_) {
            llama_model_free(model_);
            model_ = nullptr;
        }

        static bool backend_initialized = false;
        if(!backend_initialized) {
            llama_backend_init();
            llama_numa_init(settings_.numa_strategy);
            backend_initialized = true;
        }

        llama_model_params model_params = llama_model_default_params();
        model_params.n_gpu_layers = settings_.n_gpu_layers;
        model_params.use_mmap = settings_.use_mmap;

        spdlog::info("⚙️ Loading model from: {}", settings_.model_path);
        model_ = llama_model_load_from_file(settings_.model_path.c_str(), model_params);
        if (!model_) throw std::runtime_error("Failed to load model file.");
        
        char arch_buffer[64] = {0};
        llama_model_meta_val_str(model_, "general.architecture", arch_buffer, sizeof(arch_buffer));
        std::string arch = arch_buffer[0] ? arch_buffer : "unknown";
        spdlog::info("🔍 Architecture: {} (Refined Engine Active)", arch);

        context_pool_ = std::make_unique<LlamaContextPool>(settings_, model_, active_contexts_gauge_);
        
        model_loaded_ = true;
        spdlog::info("✅ Model update successful. Unlock.");
        return true;
    } catch (const std::exception& e) {
        spdlog::error("❌ Update failed: {}", e.what());
        return false;
    }
}

bool LLMEngine::is_model_loaded() const { 
    std::shared_lock<std::shared_mutex> lock(model_mutex_);
    return model_loaded_.load(); 
}

// --- LORA MANAGEMENT ---

struct llama_adapter_lora* LLMEngine::get_or_load_adapter(const std::string& lora_id) {
    std::lock_guard<std::mutex> lock(lora_mutex_);
    
    if (lora_cache_.find(lora_id) != lora_cache_.end()) {
        return lora_cache_[lora_id];
    }

    std::string sanitized_id = lora_id;
    sanitized_id.erase(std::remove(sanitized_id.begin(), sanitized_id.end(), '/'), sanitized_id.end());
    sanitized_id.erase(std::remove(sanitized_id.begin(), sanitized_id.end(), '\\'), sanitized_id.end());
    if (sanitized_id.find("..") != std::string::npos) {
        spdlog::error("🚨 Security Violation: Path traversal in LoRA ID '{}'", lora_id);
        return nullptr;
    }

    namespace fs = std::filesystem;
    fs::path lora_path = fs::path(settings_.lora_dir) / (sanitized_id + ".bin");
    
    if (!fs::exists(lora_path)) {
        lora_path = fs::path(settings_.lora_dir) / (sanitized_id + ".gguf");
    }

    if (!fs::exists(lora_path)) {
        spdlog::error("❌ LoRA adapter not found: {}", lora_path.string());
        return nullptr;
    }

    spdlog::info("💾 Loading LoRA adapter from file: {}", lora_path.string());
    struct llama_adapter_lora* adapter = llama_adapter_lora_init(model_, lora_path.c_str());
    
    if (adapter) {
        lora_cache_[sanitized_id] = adapter;
        return adapter;
    } else {
        spdlog::error("❌ Failed to init LoRA adapter: {}", sanitized_id);
        return nullptr;
    }
}

bool LLMEngine::apply_lora_to_context(llama_context* ctx, const std::string& lora_adapter_id) {
    if (lora_adapter_id.empty()) return true;

    auto* adapter = get_or_load_adapter(lora_adapter_id);
    if (!adapter) return false;

    int32_t err = llama_set_adapter_lora(ctx, adapter, 1.0f);
    if (err != 0) {
        spdlog::error("❌ Failed to apply LoRA to context. Error: {}", err);
        return false;
    }
    
    spdlog::debug("🎨 LoRA '{}' applied to context.", lora_adapter_id);
    return true;
}

void LLMEngine::clear_lora_from_context(llama_context* ctx) {
    llama_clear_adapter_lora(ctx);
    spdlog::debug("🧹 Context LoRA adapters cleared.");
}

void LLMEngine::clear_adapter_cache() {
    std::lock_guard<std::mutex> lock(lora_mutex_);
    for (auto& [id, adapter] : lora_cache_) {
        if (adapter) {
            llama_adapter_lora_free(adapter);
        }
    }
    lora_cache_.clear();
}

// --- REQUEST PROCESSING ---

void LLMEngine::process_single_request(std::shared_ptr<BatchedRequest> batched_request) {
    if (batcher_) {
        batcher_->add_request(batched_request); 
    } else {
        execute_single_request(batched_request);
    }
}

void LLMEngine::process_batch(std::vector<std::shared_ptr<BatchedRequest>>& batch) {
    if (batch.empty()) return;
    std::shared_lock<std::shared_mutex> lock(model_mutex_);
    if (!model_loaded_) return;
    
    std::vector<std::future<void>> futures;
    futures.reserve(batch.size());

    for (auto& req_ptr : batch) {
        futures.push_back(std::async(std::launch::async, [this, req_ptr]() {
            this->execute_single_request(req_ptr);
        }));
    }
    
    for (auto& f : futures) {
        f.get();
    }
}

// --- TOKENIZATION & TRUNCATION ---
std::vector<llama_token> LLMEngine::tokenize_and_truncate(std::shared_ptr<BatchedRequest> req_ptr, const std::string& formatted_prompt) {
    const auto* vocab = llama_model_get_vocab(model_);
    
    // [FIX] Gemma gibi modeller için BOS token (add_special = true) ŞARTTIR.
    // Aksi takdirde model prompt'u sızdırır (leak) veya halüsinasyon görür.
    bool add_special = true; 
    
    std::vector<llama_token> tokens(formatted_prompt.length() + 64);
    int n_tokens = llama_tokenize(vocab, formatted_prompt.c_str(), formatted_prompt.length(), tokens.data(), tokens.size(), add_special, true);
    if (n_tokens < 0) {
        tokens.resize(-n_tokens);
        n_tokens = llama_tokenize(vocab, formatted_prompt.c_str(), formatted_prompt.length(), tokens.data(), tokens.size(), add_special, true);
    }
    tokens.resize(n_tokens);

    const auto& params = req_ptr->request.params();
    uint32_t req_max_gen = params.has_max_new_tokens() ? params.max_new_tokens() : settings_.default_max_tokens;
    uint32_t max_context = settings_.context_size;
    uint32_t buffer = 64; 
    
    uint32_t effective_max_gen = std::min(req_max_gen, max_context - buffer);
    if (effective_max_gen == 0) effective_max_gen = buffer;
    uint32_t safe_prompt_limit = (max_context > effective_max_gen + buffer) ? (max_context - effective_max_gen - buffer) : 0;
    
    if (safe_prompt_limit > 0 && tokens.size() > safe_prompt_limit) {
        const size_t HEAD_SIZE = 256;
        if (safe_prompt_limit > HEAD_SIZE) {
             size_t tail_size = safe_prompt_limit - HEAD_SIZE;
             std::vector<llama_token> smart_tokens;
             smart_tokens.reserve(safe_prompt_limit);
             // BOS token'ı korumak için begin() + 1 yapmıyoruz, baştan kesiyoruz.
             smart_tokens.insert(smart_tokens.end(), tokens.begin(), tokens.begin() + HEAD_SIZE);
             smart_tokens.insert(smart_tokens.end(), tokens.end() - tail_size, tokens.end());
             tokens = std::move(smart_tokens);
        } else {
             std::vector<llama_token> truncated_tokens(tokens.begin() + (tokens.size() - safe_prompt_limit), tokens.end());
             tokens = std::move(truncated_tokens);
        }
    }
    
    req_ptr->prompt_tokens = tokens.size();
    return tokens;
}

// --- CORE LOGIC: DECODING ---

bool LLMEngine::decode_prompt(llama_context* ctx, ContextGuard& guard, const std::vector<llama_token>& prompt_tokens, std::shared_ptr<BatchedRequest> req_ptr) {
    size_t matched_len = guard.get_matched_tokens();
    llama_memory_seq_rm(llama_get_memory(ctx), -1, matched_len, -1);
    
    size_t tokens_to_process = prompt_tokens.size() - matched_len;
    if (tokens_to_process > 0) {
        int32_t n_batch = llama_n_batch(ctx);
        LlamaBatchScope batch_scope(n_batch, 0, 1);
        
        for (size_t i = 0; i < tokens_to_process; i += n_batch) {
            batch_scope.clear();
            int32_t n_eval = std::min((int32_t)(tokens_to_process - i), n_batch);
            
            for(int j = 0; j < n_eval; ++j) {
                common_batch_add(batch_scope.batch, prompt_tokens[matched_len + i + j], matched_len + i + j, {0}, false);
            }
            if (i + n_eval == tokens_to_process) batch_scope.batch.logits[n_eval - 1] = true;
            
            if (llama_decode(ctx, batch_scope.batch) != 0) {
                req_ptr->finish_reason = "length_error";
                return false;
            }
        }
    }
    return true;
}

// --- CORE LOGIC: GENERATION ---

void LLMEngine::generate_response(llama_context* ctx, const std::vector<llama_token>& prompt_tokens, std::shared_ptr<BatchedRequest> req_ptr) {
    const auto* vocab = llama_model_get_vocab(model_);
    const auto& params = req_ptr->request.params();
    
    LlamaSamplerGuard sampler_guard(llama_sampler_chain_default_params());
    llama_sampler* chain = sampler_guard.sampler;

    if (!req_ptr->grammar.empty()) {
        llama_sampler* g = llama_sampler_init_grammar(vocab, req_ptr->grammar.c_str(), "root");
        if (g) llama_sampler_chain_add(chain, g);
    }
    
    float repeat_penalty = params.has_repetition_penalty() ? params.repetition_penalty() : settings_.default_repeat_penalty;
    llama_sampler_chain_add(chain, llama_sampler_init_penalties(llama_n_ctx(ctx), repeat_penalty, 0.0f, 0.0f));
    llama_sampler_chain_add(chain, llama_sampler_init_top_k(params.has_top_k() ? params.top_k() : settings_.default_top_k));
    llama_sampler_chain_add(chain, llama_sampler_init_temp(params.has_temperature() ? params.temperature() : settings_.default_temperature));
    llama_sampler_chain_add(chain, llama_sampler_init_dist(time(NULL)));

    uint32_t req_max_gen = params.has_max_new_tokens() ? params.max_new_tokens() : settings_.default_max_tokens;
    int n_decoded = 0;
    llama_pos n_past = prompt_tokens.size();
    
    LlamaBatchScope token_batch(1, 0, 1);
    
    while (n_decoded < (int)req_max_gen) {
        if (req_ptr->should_stop_callback && req_ptr->should_stop_callback()) {
            req_ptr->finish_reason = "cancelled"; break;
        }
        
        llama_token new_token_id = llama_sampler_sample(chain, ctx, -1);
        llama_sampler_accept(chain, new_token_id);
        
        if (llama_vocab_is_eog(vocab, new_token_id)) {
            req_ptr->finish_reason = "stop"; break;
        }

        char buf[256] = {0};
        int n = llama_token_to_piece(vocab, new_token_id, buf, sizeof(buf), 0, true); 
        std::string token_str(buf, n);
        
        if(req_ptr->on_token_callback) req_ptr->on_token_callback(token_str);
        req_ptr->token_queue.push(token_str);
        
        token_batch.clear();
        common_batch_add(token_batch.batch, new_token_id, n_past, {0}, true);
        
        if(llama_decode(ctx, token_batch.batch) != 0) {
            req_ptr->finish_reason = "context_full"; break;
        }
        n_past++;
        n_decoded++;
    }
    
    req_ptr->completion_tokens = n_decoded;
    if (req_ptr->finish_reason.empty()) req_ptr->finish_reason = "length";
    req_ptr->prompt_tokens = prompt_tokens.size(); 
}

void LLMEngine::execute_single_request(std::shared_ptr<BatchedRequest> req_ptr) {
    std::optional<ContextGuard> guard;
    llama_context* ctx = nullptr;

    try {
        std::string formatted_prompt = formatter_->format(req_ptr->request, settings_);
        std::vector<llama_token> prompt_tokens = tokenize_and_truncate(req_ptr, formatted_prompt);
        
        // 1. Context'i SADECE tokenları hesapladıktan sonra edin
        guard.emplace(context_pool_->acquire(prompt_tokens));
        ctx = guard->get();
        
        // 2. LoRA Uygula
        bool lora_active = false;
        if (req_ptr->request.has_lora_adapter_id()) {
            lora_active = apply_lora_to_context(ctx, req_ptr->request.lora_adapter_id());
        }

        // 3. İşlem
        if (decode_prompt(ctx, *guard, prompt_tokens, req_ptr)) {
            generate_response(ctx, prompt_tokens, req_ptr);
        }
        
        // 4. Temizlik
        if (lora_active) {
            clear_lora_from_context(ctx);
        }

        guard->release_early(prompt_tokens);

    } catch (const std::exception& e) {
        spdlog::error("Error executing request: {}", e.what());
        req_ptr->finish_reason = "error";
        if (ctx) clear_lora_from_context(ctx);
    }
}
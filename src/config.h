#pragma once

#include <string>
#include <cstdlib>
#include <thread>
#include <stdexcept>
#include "spdlog/spdlog.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include "nlohmann/json.hpp"
#include "llama.h"

// ==================================================================================
// ⚙️ GLOBAL SETTINGS STRUCTURE
// ==================================================================================
struct Settings {
    // --- NETWORK & CONCURRENCY ---
    std::string host = "0.0.0.0";
    int http_port = 16070;
    int grpc_port = 16071;
    int metrics_port = 16072;
    int http_threads = 50; 

    // --- MODEL IDENTIFICATION ---
    std::string profile_name = "default"; 
    std::string model_dir = "/models";
    std::string lora_dir = "/lora_adapters";
    std::string model_id = "";
    std::string model_filename = "";
    std::string model_path = "";
    std::string legacy_model_path = "";
    std::string model_url_template = "https://huggingface.co/{model_id}/resolve/main/{filename}";
    
    // --- TEMPLATE CONFIGURATION (TURKISH DEFAULT / ADAPTIVE) ---
    // [ADAPTIVE] Türkçe varsayılan, İngilizce algılandığında geçiş yapan akıllı prompt
    std::string template_system_prompt = 
        "Sen 'Sentirik'sin. Profesyonel, yardımsever ve zeki bir asistansın.\n"
        "\n"
        "### İLETİŞİM PROTOKOLLERİ:\n"
        "1. DİL: Varsayılan dilin Türkçe'dir. Ancak kullanıcı İngilizce veya başka bir dilde yazarsa, algıladığın dilde yanıt ver.\n"
        "2. ÜSLUP: Doğal, akıcı ve insan benzeri konuş. Robotik kalıplardan (Örn: 'Bir yapay zeka olarak...') kaçın.\n"
        "3. RAG: Eğer [BİLGİ] bloğu verilmişse, cevabı SADECE oradaki veriye dayandır.\n"
        "4. GÜVENLİK: Tıbbi/Hayati acil durumlarda yorum yapma, doğrudan 112/Doktora yönlendir.";

    std::string template_rag_prompt = 
        "[BİLGİ]\n{{rag_context}}\n[BİLGİ SONU]\n\n"
        "Yukarıdaki bağlamı kullanarak cevapla:\nSoru: {{user_prompt}}";
    
    std::string reasoning_prompt_low = "\n[TALİMAT]: Cevap vermeden önce kısaca düşün. Düşüncelerini <think> etiketleri içine al.";
    std::string reasoning_prompt_high = "\n[TALİMAT]: Bu karmaşık bir görev. Adım adım derinlemesine düşün. Analizini <think> etiketleri içine al.";

    // --- ENGINE & HARDWARE PERFORMANCE (ECO MODE DEFAULTS) ---
    int n_gpu_layers = 100;            // Default: All Layers on GPU (Speed)
    uint32_t context_size = 2048;      // Default: 2048 (VRAM Safety for Multi-Service)
    
    // Threading Strategy (Limited to 4 to save CPU for STT/TTS)
    // Hardware concurrency genellikle sanal çekirdekleri de sayar, 4 güvenli bir limittir.
    uint32_t n_threads = std::max(1u, std::min(4u, std::thread::hardware_concurrency()));
    uint32_t n_threads_batch = std::max(1u, std::min(4u, std::thread::hardware_concurrency())); 
    
    // Batching & Memory
    uint32_t physical_batch_size = 512; 
    ggml_numa_strategy numa_strategy = GGML_NUMA_STRATEGY_DISABLED;
    bool use_mmap = true;
    bool kv_offload = true;
    
    // Dynamic Batching (Request Queue)
    bool enable_dynamic_batching = true;
    size_t max_batch_size = 1;          // Default: 1 (Strict Memory Limit)
    int batch_timeout_ms = 5;
    bool enable_warm_up = true;

    // --- LOGGING & SECURITY ---
    std::string log_level = "info";
    std::string grpc_ca_path = "";
    std::string grpc_cert_path = "";
    std::string grpc_key_path = "";

    // --- SAMPLING DEFAULTS ---
    float default_temperature = 0.2f;
    int32_t default_top_k = 40;
    float default_top_p = 0.95f;
    float default_repeat_penalty = 1.1f;
    int32_t default_max_tokens = 1024;

    // --- GATEWAY INTEGRATION ---
    std::string worker_id = "worker-default";
    std::string worker_group = "default-group";
    std::string gateway_address = "";

    // JSON Serialization (FULL OBSERVABILITY RECTIFIED)
    nlohmann::json to_json() const {
        return {
            // Kimlik
            {"profile_name", profile_name},
            {"model_id", model_id},
            
            // Donanım Kaynakları
            {"gpu_layers", n_gpu_layers},
            {"context_size", context_size},
            {"threads", n_threads},
            {"threads_batch", n_threads_batch},       // [RESTORED]
            {"physical_batch_size", physical_batch_size}, // [RESTORED]
            
            // Eşzamanlılık & Bellek
            {"max_batch_size_slots", max_batch_size},
            {"kv_offload", kv_offload},
            {"use_mmap", use_mmap},                   // [RESTORED]
            {"enable_dynamic_batching", enable_dynamic_batching}, // [RESTORED]
            
            // Promptlar
            {"template_system_prompt", template_system_prompt}
        };
    }
};

// ==================================================================================
// 🛠️ HELPER: PROFILE LOADER
// ==================================================================================
inline bool apply_profile(Settings& s, const std::string& profile_name_override) {
    namespace fs = std::filesystem;
    using json = nlohmann::json;

    fs::path profile_path = "profiles.json"; 
    
    // Fallback paths
    if (!fs::exists(profile_path)) {
        profile_path = fs::path(s.model_dir) / "profiles.json";
        if (!fs::exists(profile_path)) {
            spdlog::debug("Profiles file not found. Using defaults/env only.");
            return false;
        }
    }

    try {
        std::ifstream f(profile_path);
        json j = json::parse(f);
        
        std::string active = profile_name_override;
        if (active.empty()) {
            active = j.value("active_profile", "default");
        }
        
        if (j.contains("profiles") && j["profiles"].contains(active)) {
            auto& p = j["profiles"][active];
            spdlog::info("📂 [Config] Loading Profile: '{}'", active);
            
            s.profile_name = active;
            
            // --- Model Identity ---
            if(p.contains("model_id")) s.model_id = p["model_id"];
            if(p.contains("filename")) s.model_filename = p["filename"];
            
            // --- Hardware & Performance ---
            if(p.contains("context_size")) s.context_size = p["context_size"];
            if(p.contains("gpu_layers")) s.n_gpu_layers = p["gpu_layers"];
            if(p.contains("threads")) s.n_threads = p["threads"];
            if(p.contains("threads_batch")) s.n_threads_batch = p["threads_batch"];
            if(p.contains("physical_batch_size")) s.physical_batch_size = p["physical_batch_size"];
            if(p.contains("max_batch_size")) s.max_batch_size = p["max_batch_size"];
            
            // --- Flags ---
            if(p.contains("use_mmap")) s.use_mmap = p["use_mmap"];
            if(p.contains("kv_offload")) s.kv_offload = p["kv_offload"];
            if(p.contains("enable_batching")) s.enable_dynamic_batching = p["enable_batching"];

            // --- Sampling Defaults ---
            if(p.contains("temperature")) s.default_temperature = p["temperature"];
            if(p.contains("top_k")) s.default_top_k = p["top_k"];
            if(p.contains("top_p")) s.default_top_p = p["top_p"];
            if(p.contains("repeat_penalty")) s.default_repeat_penalty = p["repeat_penalty"];
            
            // --- Templates ---
            if (p.contains("templates")) {
                const auto& templates = p["templates"];
                s.template_system_prompt = templates.value("system_prompt", s.template_system_prompt);
                s.template_rag_prompt = templates.value("rag_prompt", s.template_rag_prompt);
            }
            
            return true;
        } else {
            spdlog::warn("⚠️ Requested profile '{}' not found in profiles.json. Falling back to defaults.", active);
            return false;
        }
    } catch (const std::exception& e) {
        spdlog::error("❌ Failed to parse profiles.json: {}", e.what());
        return false;
    }
}

// ==================================================================================
// 🚀 MAIN CONFIG LOADER
// Strategy: Defaults -> Profile -> Environment Variables (Highest Priority)
// ==================================================================================
inline Settings load_settings() {
    Settings s;

    // 1. Helper Lambdas for ENV reading with Logging
    auto get_env_var = [](const char* name, const std::string& default_val) -> std::string {
        const char* val = std::getenv(name);
        return val ? std::string(val) : default_val;
    };
    
    // Env variable varsa döndür, yoksa mevcut değeri (current_val) koru
    auto override_int = [&](const char* name, int& current_val) {
        const char* val_str = std::getenv(name);
        if (val_str) {
            try { 
                current_val = std::stoi(val_str); 
                spdlog::info("🔧 [Env Override] {} = {}", name, current_val);
            } catch (...) {}
        }
    };
    auto override_uint = [&](const char* name, uint32_t& current_val) {
        const char* val_str = std::getenv(name);
        if (val_str) {
            try { 
                current_val = static_cast<uint32_t>(std::stoul(val_str)); 
                spdlog::info("🔧 [Env Override] {} = {}", name, current_val);
            } catch (...) {}
        }
    };
    // Size_t için özel (batch size vb.)
    auto override_size = [&](const char* name, size_t& current_val) {
        const char* val_str = std::getenv(name);
        if (val_str) {
            try { 
                current_val = static_cast<size_t>(std::stoul(val_str)); 
                spdlog::info("🔧 [Env Override] {} = {}", name, current_val);
            } catch (...) {}
        }
    };
    auto override_float = [&](const char* name, float& current_val) {
        const char* val_str = std::getenv(name);
        if (val_str) {
            try { 
                current_val = std::stof(val_str); 
                spdlog::info("🔧 [Env Override] {} = {}", name, current_val);
            } catch (...) {}
        }
    };
    auto override_bool = [&](const char* name, bool& current_val) {
        const char* val_str = std::getenv(name);
        if (val_str) {
            std::string val = val_str;
            std::transform(val.begin(), val.end(), val.begin(), ::tolower);
            current_val = (val == "true" || val == "1");
            spdlog::info("🔧 [Env Override] {} = {}", name, current_val ? "true" : "false");
        }
    };
    auto override_string = [&](const char* name, std::string& current_val) {
        const char* val = std::getenv(name);
        if (val) {
            current_val = std::string(val);
            spdlog::info("🔧 [Env Override] {} = '{}'", name, current_val);
        }
    };

    // 2. Load basic paths first
    override_string("LLM_LLAMA_SERVICE_MODEL_DIR", s.model_dir);
    
    // 3. APPLY PROFILE (Overwrites defaults)
    apply_profile(s, ""); 

    // 4. APPLY ENVIRONMENT VARIABLES (Overwrites Profile)
    
    // Network
    override_string("LLM_LLAMA_SERVICE_LISTEN_ADDRESS", s.host);
    override_int("LLM_LLAMA_SERVICE_HTTP_PORT", s.http_port);
    override_int("LLM_LLAMA_SERVICE_GRPC_PORT", s.grpc_port);
    override_int("LLM_LLAMA_SERVICE_METRICS_PORT", s.metrics_port);
    override_int("LLM_LLAMA_SERVICE_HTTP_THREADS", s.http_threads);

    // Model & Paths
    override_string("LLM_LLAMA_SERVICE_LORA_DIR", s.lora_dir);
    override_string("LLM_LLAMA_SERVICE_MODEL_ID", s.model_id);
    override_string("LLM_LLAMA_SERVICE_MODEL_FILENAME", s.model_filename);
    
    // Hardware & Performance
    override_int("LLM_LLAMA_SERVICE_GPU_LAYERS", s.n_gpu_layers);
    override_uint("LLM_LLAMA_SERVICE_CONTEXT_SIZE", s.context_size);
    override_uint("LLM_LLAMA_SERVICE_THREADS", s.n_threads);
    override_uint("LLM_LLAMA_SERVICE_THREADS_BATCH", s.n_threads_batch);
    override_bool("LLM_LLAMA_SERVICE_USE_MMAP", s.use_mmap);
    override_bool("LLM_LLAMA_SERVICE_KV_OFFLOAD", s.kv_offload);
    
    // Batching & Concurrency
    override_bool("LLM_LLAMA_SERVICE_ENABLE_BATCHING", s.enable_dynamic_batching);
    override_size("LLM_LLAMA_SERVICE_MAX_BATCH_SIZE", s.max_batch_size);
    override_int("LLM_LLAMA_SERVICE_BATCH_TIMEOUT_MS", s.batch_timeout_ms);
    override_uint("LLM_LLAMA_SERVICE_PHYSICAL_BATCH_SIZE", s.physical_batch_size);

    // Logging & Security
    override_string("LLM_LLAMA_SERVICE_LOG_LEVEL", s.log_level);
    override_string("GRPC_TLS_CA_PATH", s.grpc_ca_path);
    override_string("LLM_LLAMA_SERVICE_CERT_PATH", s.grpc_cert_path);
    override_string("LLM_LLAMA_SERVICE_KEY_PATH", s.grpc_key_path);

    // Sampling Defaults
    override_int("LLM_LLAMA_SERVICE_DEFAULT_MAX_TOKENS", s.default_max_tokens);
    override_float("LLM_LLAMA_SERVICE_DEFAULT_TEMPERATURE", s.default_temperature);
    override_int("LLM_LLAMA_SERVICE_DEFAULT_TOP_K", s.default_top_k);
    override_float("LLM_LLAMA_SERVICE_DEFAULT_TOP_P", s.default_top_p);
    override_float("LLM_LLAMA_SERVICE_DEFAULT_REPEAT_PENALTY", s.default_repeat_penalty);
    override_string("LLM_LLAMA_SERVICE_DEFAULT_SYSTEM_PROMPT", s.template_system_prompt);
    override_string("LLM_LLAMA_SERVICE_DEFAULT_RAG_PROMPT", s.template_rag_prompt);
    override_string("LLM_LLAMA_SERVICE_DEFAULT_RAG_PROMPT_LOW", s.reasoning_prompt_low);
    override_string("LLM_LLAMA_SERVICE_DEFAULT_RAG_PROMPT_HIGH", s.reasoning_prompt_high);

    // Gateway
    s.worker_id = get_env_var("LLM_WORKKER_ID", "worker-" + std::to_string(std::rand()));
    override_string("LLM_WORKER_GROUP", s.worker_group);
    override_string("LLM_GATEWAY_ADDRESS", s.gateway_address);
    
    // Legacy support
    override_string("LLM_LLAMA_SERVICE_MODEL_PATH", s.legacy_model_path);
    if (s.model_id.empty() && !s.legacy_model_path.empty()) {
        std::filesystem::path p(s.legacy_model_path);
        s.model_filename = p.filename().string();
    }

    // 5. Sanity Checks
    if (s.context_size < 512) {
        spdlog::warn("⚠️ Context size {} is too low. Forcing minimum 512.", s.context_size);
        s.context_size = 512;
    }
    if (s.max_batch_size < 1) s.max_batch_size = 1;

    return s;
};
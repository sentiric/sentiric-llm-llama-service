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

struct Settings {
    // Network
    std::string host = "0.0.0.0";
    int http_port = 16070;
    int grpc_port = 16071;
    int metrics_port = 16072;

    // Model & Profiles
    std::string profile_name = "default";
    std::string model_dir = "/models";
    std::string model_id = "";
    std::string model_filename = "";
    std::string model_path = "";
    std::string legacy_model_path = "";
    std::string system_prompt = "";

    // Engine & Performance
    int n_gpu_layers = 0;
    uint32_t context_size = 4096;
    uint32_t n_threads = std::max(1u, std::min(8u, std::thread::hardware_concurrency()));
    uint32_t n_threads_batch = std::max(1u, std::min(8u, std::thread::hardware_concurrency()));
    ggml_numa_strategy numa_strategy = GGML_NUMA_STRATEGY_DISABLED;
    bool use_mmap = true;
    bool kv_offload = true;
    
    // Logging
    std::string log_level = "info";

    // Sampling Defaults
    float default_temperature = 0.8f;
    int32_t default_top_k = 40;
    float default_top_p = 0.95f;
    float default_repeat_penalty = 1.1f;
    int32_t default_max_tokens = 1024;

    // Security (mTLS)
    std::string grpc_ca_path = "";
    std::string grpc_cert_path = "";
    std::string grpc_key_path = "";
    
    // Dynamic Batching
    bool enable_dynamic_batching = true;
    size_t max_batch_size = 8;
    int batch_timeout_ms = 5;
    
    // Warm-up
    bool enable_warm_up = true;

    // Gateway
    std::string worker_id = "worker-default";
    std::string worker_group = "default-group";
    std::string gateway_address = "";

    // Serialization for UI
    nlohmann::json to_json() const {
        return {
            {"gpu_layers", n_gpu_layers},
            {"context_size", context_size},
            {"threads", n_threads},
            {"batch_size", max_batch_size},
            {"kv_offload", kv_offload},
            {"use_mmap", use_mmap}
        };
    }
};

// Profil dosyasını okuyup ayarları güncelleyen yardımcı fonksiyon.
inline bool apply_profile(Settings& s, const std::string& profile_name_override) {
    namespace fs = std::filesystem;
    using json = nlohmann::json;

    fs::path profile_path = "profiles.json"; 
    
    if (!fs::exists(profile_path)) {
        profile_path = fs::path(s.model_dir) / "profiles.json";
        if (!fs::exists(profile_path)) {
            spdlog::warn("Profiles file not found at ./profiles.json or {}. Relying on env vars.", profile_path.string());
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
            spdlog::info("🔄 Loading Settings from Profile: [{}] (Source: {})", active, profile_path.string());
            
            s.profile_name = active;
            
            if(p.contains("model_id")) s.model_id = p["model_id"];
            if(p.contains("filename")) s.model_filename = p["filename"];
            if(p.contains("context_size")) s.context_size = p["context_size"];
            if(p.contains("gpu_layers")) s.n_gpu_layers = p["gpu_layers"];
            if(p.contains("temperature")) s.default_temperature = p["temperature"];
            if(p.contains("system_prompt")) s.system_prompt = p["system_prompt"];
            if(p.contains("use_mmap")) s.use_mmap = p["use_mmap"];
            if(p.contains("kv_offload")) s.kv_offload = p["kv_offload"];
            
            return true;
        } else {
            spdlog::error("Requested profile '{}' not found in profiles.json.", active);
            return false;
        }
    } catch (const std::exception& e) {
        spdlog::error("Failed to parse profiles.json: {}", e.what());
        return false;
    }
}

inline Settings load_settings() {
    Settings s;
    auto get_env_var = [](const char* name, const std::string& default_val) -> std::string {
        const char* val = std::getenv(name);
        return val ? std::string(val) : default_val;
    };
    auto get_env_var_as_int = [&](const char* name, int default_val) -> int {
        const char* val_str = std::getenv(name);
        if (!val_str) return default_val;
        try { return std::stoi(val_str); } catch (...) { return default_val; }
    };
    auto get_env_var_as_uint = [&](const char* name, uint32_t default_val) -> uint32_t {
        const char* val_str = std::getenv(name);
        if (!val_str) return default_val;
        try { return static_cast<uint32_t>(std::stoul(val_str)); } catch (...) { return default_val; }
    };
    auto get_env_var_as_float = [&](const char* name, float default_val) -> float {
        const char* val_str = std::getenv(name);
        if (!val_str) return default_val;
        try { return std::stof(val_str); } catch (...) { return default_val; }
    };
    auto get_env_var_as_bool = [&](const char* name, bool default_val) -> bool {
        const char* val_str = std::getenv(name);
        if (!val_str) return default_val;
        std::string val = val_str;
        std::transform(val.begin(), val.end(), val.begin(), ::tolower);
        return val == "true" || val == "1";
    };
    
    // Base Environment Configs
    s.host = get_env_var("LLM_LLAMA_SERVICE_LISTEN_ADDRESS", s.host);
    s.http_port = get_env_var_as_int("LLM_LLAMA_SERVICE_HTTP_PORT", s.http_port);
    s.grpc_port = get_env_var_as_int("LLM_LLAMA_SERVICE_GRPC_PORT", s.grpc_port);
    s.metrics_port = get_env_var_as_int("LLM_LLAMA_SERVICE_METRICS_PORT", s.metrics_port);
    s.model_dir = get_env_var("LLM_LLAMA_SERVICE_MODEL_DIR", s.model_dir);
    
    // Default values from Env
    s.model_id = get_env_var("LLM_LLAMA_SERVICE_MODEL_ID", s.model_id);
    s.model_filename = get_env_var("LLM_LLAMA_SERVICE_MODEL_FILENAME", s.model_filename);
    s.n_gpu_layers = get_env_var_as_int("LLM_LLAMA_SERVICE_GPU_LAYERS", s.n_gpu_layers);
    s.context_size = get_env_var_as_uint("LLM_LLAMA_SERVICE_CONTEXT_SIZE", s.context_size);
    s.n_threads = get_env_var_as_uint("LLM_LLAMA_SERVICE_THREADS", s.n_threads);
    s.n_threads_batch = get_env_var_as_uint("LLM_LLAMA_SERVICE_THREADS_BATCH", s.n_threads_batch);
    s.use_mmap = get_env_var_as_bool("LLM_LLAMA_SERVICE_USE_MMAP", s.use_mmap);
    s.kv_offload = get_env_var_as_bool("LLM_LLAMA_SERVICE_KV_OFFLOAD", s.kv_offload);
    s.log_level = get_env_var("LLM_LLAMA_SERVICE_LOG_LEVEL", s.log_level);

    s.default_max_tokens = get_env_var_as_int("LLM_LLAMA_SERVICE_DEFAULT_MAX_TOKENS", s.default_max_tokens);
    s.default_temperature = get_env_var_as_float("LLM_LLAMA_SERVICE_DEFAULT_TEMPERATURE", s.default_temperature);
    s.default_top_k = get_env_var_as_int("LLM_LLAMA_SERVICE_DEFAULT_TOP_K", s.default_top_k);
    s.default_top_p = get_env_var_as_float("LLM_LLAMA_SERVICE_DEFAULT_TOP_P", s.default_top_p);
    s.default_repeat_penalty = get_env_var_as_float("LLM_LLAMA_SERVICE_DEFAULT_REPEAT_PENALTY", s.default_repeat_penalty);

    s.grpc_ca_path = get_env_var("GRPC_TLS_CA_PATH", s.grpc_ca_path);
    s.grpc_cert_path = get_env_var("LLM_LLAMA_SERVICE_CERT_PATH", s.grpc_cert_path);
    s.grpc_key_path = get_env_var("LLM_LLAMA_SERVICE_KEY_PATH", s.grpc_key_path);
    
    s.enable_dynamic_batching = get_env_var_as_bool("LLM_LLAMA_SERVICE_ENABLE_BATCHING", s.enable_dynamic_batching);
    s.max_batch_size = get_env_var_as_uint("LLM_LLAMA_SERVICE_MAX_BATCH_SIZE", s.max_batch_size);
    s.batch_timeout_ms = get_env_var_as_int("LLM_LLAMA_SERVICE_BATCH_TIMEOUT_MS", s.batch_timeout_ms);
    s.enable_warm_up = get_env_var_as_bool("LLM_LLAMA_SERVICE_ENABLE_WARM_UP", s.enable_warm_up);

    s.worker_id = get_env_var("LLM_WORKER_ID", "worker-" + std::to_string(std::rand()));
    s.worker_group = get_env_var("LLM_WORKER_GROUP", "default");
    s.gateway_address = get_env_var("LLM_GATEWAY_ADDRESS", "");
    
    s.legacy_model_path = get_env_var("LLM_LLAMA_SERVICE_MODEL_PATH", s.legacy_model_path);
    if (s.model_id.empty() && !s.legacy_model_path.empty()) {
        std::filesystem::path p(s.legacy_model_path);
        s.model_filename = p.filename().string();
    }

    // Profil dosyasını uygula
    apply_profile(s, ""); 

    return s;
};
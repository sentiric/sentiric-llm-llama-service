// src/config.h
#pragma once

#include <string>
#include <cstdlib>
#include <thread>
#include <stdexcept>
#include "spdlog/spdlog.h"
#include <algorithm>
#include <filesystem>
#include "llama.h"

struct Settings {
    // Network
    std::string host = "0.0.0.0";
    int http_port = 16070;
    int grpc_port = 16071;
    int metrics_port = 16072;

    // Model
    std::string model_dir = "/models";
    std::string model_id = "";
    std::string model_filename = "";
    std::string model_path = "";
    std::string legacy_model_path = "";

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
    
    // Dynamic Batching ayarları
    bool enable_dynamic_batching = true;
    size_t max_batch_size = 8;
    int batch_timeout_ms = 5;
    
    // Warm-up ayarları
    bool enable_warm_up = true;
};

inline Settings load_settings() {
    Settings s;
    auto get_env_var = [](const char* name, const std::string& default_val) -> std::string {
        const char* val = std::getenv(name);
        return val ? std::string(val) : default_val;
    };
    auto get_env_var_as_int = [&](const char* name, int default_val) -> int {
        const char* val_str = std::getenv(name);
        if (!val_str) return default_val;
        try { return std::stoi(val_str); } catch (const std::exception& e) {
            spdlog::warn("Invalid integer for env var '{}': {}. Using default {}.", name, e.what(), default_val);
            return default_val;
        }
    };
    auto get_env_var_as_uint = [&](const char* name, uint32_t default_val) -> uint32_t {
        const char* val_str = std::getenv(name);
        if (!val_str) return default_val;
        try { return static_cast<uint32_t>(std::stoul(val_str)); } catch (const std::exception& e) {
            spdlog::warn("Invalid unsigned integer for env var '{}': {}. Using default {}.", name, e.what(), default_val);
            return default_val;
        }
    };
    auto get_env_var_as_float = [&](const char* name, float default_val) -> float {
        const char* val_str = std::getenv(name);
        if (!val_str) return default_val;
        try { return std::stof(val_str); } catch (const std::exception& e) {
            spdlog::warn("Invalid float for env var '{}': {}. Using default {}.", name, e.what(), default_val);
            return default_val;
        }
    };
    auto get_env_var_as_bool = [&](const char* name, bool default_val) -> bool {
        const char* val_str = std::getenv(name);
        if (!val_str) return default_val;
        std::string val = val_str;
        std::transform(val.begin(), val.end(), val.begin(), ::tolower);
        return val == "true" || val == "1";
    };
    
    // Network
    s.host = get_env_var("LLM_LLAMA_SERVICE_LISTEN_ADDRESS", s.host);
    s.http_port = get_env_var_as_int("LLM_LLAMA_SERVICE_HTTP_PORT", s.http_port);
    s.grpc_port = get_env_var_as_int("LLM_LLAMA_SERVICE_GRPC_PORT", s.grpc_port);
    s.metrics_port = get_env_var_as_int("LLM_LLAMA_SERVICE_METRICS_PORT", s.metrics_port);
    
    // Model
    s.model_dir = get_env_var("LLM_LLAMA_SERVICE_MODEL_DIR", s.model_dir);
    s.model_id = get_env_var("LLM_LLAMA_SERVICE_MODEL_ID", s.model_id);
    s.model_filename = get_env_var("LLM_LLAMA_SERVICE_MODEL_FILENAME", s.model_filename);
    
    // Engine & Performance
    s.n_gpu_layers = get_env_var_as_int("LLM_LLAMA_SERVICE_GPU_LAYERS", s.n_gpu_layers);
    s.context_size = get_env_var_as_uint("LLM_LLAMA_SERVICE_CONTEXT_SIZE", s.context_size);
    s.n_threads = get_env_var_as_uint("LLM_LLAMA_SERVICE_THREADS", s.n_threads);
    s.n_threads_batch = get_env_var_as_uint("LLM_LLAMA_SERVICE_THREADS_BATCH", s.n_threads_batch);
    s.use_mmap = get_env_var_as_bool("LLM_LLAMA_SERVICE_USE_MMAP", s.use_mmap);
    s.kv_offload = get_env_var_as_bool("LLM_LLAMA_SERVICE_KV_OFFLOAD", s.kv_offload);
    s.numa_strategy = GGML_NUMA_STRATEGY_DISABLED;

    // Logging
    s.log_level = get_env_var("LLM_LLAMA_SERVICE_LOG_LEVEL", s.log_level);

    // Sampling Defaults
    s.default_max_tokens = get_env_var_as_int("LLM_LLAMA_SERVICE_DEFAULT_MAX_TOKENS", s.default_max_tokens);
    s.default_temperature = get_env_var_as_float("LLM_LLAMA_SERVICE_DEFAULT_TEMPERATURE", s.default_temperature);
    s.default_top_k = get_env_var_as_int("LLM_LLAMA_SERVICE_DEFAULT_TOP_K", s.default_top_k);
    s.default_top_p = get_env_var_as_float("LLM_LLAMA_SERVICE_DEFAULT_TOP_P", s.default_top_p);
    s.default_repeat_penalty = get_env_var_as_float("LLM_LLAMA_SERVICE_DEFAULT_REPEAT_PENALTY", s.default_repeat_penalty);

    // Security (mTLS)
    s.grpc_ca_path = get_env_var("GRPC_TLS_CA_PATH", s.grpc_ca_path);
    s.grpc_cert_path = get_env_var("LLM_LLAMA_SERVICE_CERT_PATH", s.grpc_cert_path);
    s.grpc_key_path = get_env_var("LLM_LLAMA_SERVICE_KEY_PATH", s.grpc_key_path);
    
    s.enable_dynamic_batching = get_env_var_as_bool("LLM_LLAMA_SERVICE_ENABLE_BATCHING", s.enable_dynamic_batching);
    s.max_batch_size = get_env_var_as_uint("LLM_LLAMA_SERVICE_MAX_BATCH_SIZE", s.max_batch_size);
    s.batch_timeout_ms = get_env_var_as_int("LLM_LLAMA_SERVICE_BATCH_TIMEOUT_MS", s.batch_timeout_ms);
    
    s.enable_warm_up = get_env_var_as_bool("LLM_LLAMA_SERVICE_ENABLE_WARM_UP", s.enable_warm_up);
    
    // Legacy path for backward compatibility
    s.legacy_model_path = get_env_var("LLM_LLAMA_SERVICE_MODEL_PATH", s.legacy_model_path);
    if (s.model_id.empty() && !s.legacy_model_path.empty()) {
        spdlog::warn("Using legacy LLM_LLAMA_SERVICE_MODEL_PATH. It's recommended to set MODEL_ID and MODEL_FILENAME instead.");
        std::filesystem::path p(s.legacy_model_path);
        s.model_filename = p.filename().string();
    }
    return s;
};
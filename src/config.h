#pragma once

#include <string>
#include <cstdlib>
#include <thread>
#include <stdexcept>
#include "spdlog/spdlog.h"
#include <algorithm>

struct Settings {
    // Network Settings
    std::string host = "0.0.0.0";
    int http_port = 16070;
    int grpc_port = 16071;

    // Model & Engine Settings
    std::string model_path = "/models/phi-3-mini.q4.gguf";
    int context_size = 4096;
    int n_threads = std::max(1u, std::min(8u, std::thread::hardware_concurrency() / 2));
    
    // Logging Settings
    std::string log_level = "info";

    // Default Sampling Parameters
    float default_temperature = 0.8f;
    int32_t default_top_k = 40;
    float default_top_p = 0.95f;
    float default_repeat_penalty = 1.1f;
    int32_t default_max_tokens = 1024;
};

inline Settings load_settings() {
    Settings s;
    auto get_env_var = [](const char* name, const std::string& default_val) -> std::string {
        const char* val = std::getenv(name);
        return val == nullptr ? default_val : std::string(val);
    };
    auto get_env_var_as_int = [&](const char* name, int default_val) -> int {
        const char* val_str = std::getenv(name);
        if (val_str == nullptr) return default_val;
        try {
            return std::stoi(val_str);
        } catch (const std::exception& e) {
            spdlog::error("Invalid integer for env var '{}': {}. Using default {}.", name, e.what(), default_val);
            return default_val;
        }
    };
    
    // Tamamen standartlaştırılmış ortam değişkenleri
    s.host = get_env_var("LLM_LLAMA_SERVICE_IPV4_ADDRESS", s.host);
    s.http_port = get_env_var_as_int("LLM_LLAMA_SERVICE_HTTP_PORT", s.http_port);
    s.grpc_port = get_env_var_as_int("LLM_LLAMA_SERVICE_GRPC_PORT", s.grpc_port);
    
    s.model_path = get_env_var("LLM_LLAMA_SERVICE_MODEL_PATH", s.model_path);
    s.context_size = get_env_var_as_int("LLM_LLAMA_SERVICE_CONTEXT_SIZE", s.context_size);
    s.n_threads = get_env_var_as_int("LLM_LLAMA_SERVICE_THREADS", s.n_threads);
    
    s.log_level = get_env_var("LLM_LLAMA_SERVICE_LOG_LEVEL", s.log_level);
    
    return s;
}
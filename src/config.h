#pragma once

#include <string>
#include <cstdlib>
#include <thread>
#include <stdexcept>
#include "spdlog/spdlog.h"

struct Settings {
    int http_port = 16060;
    int grpc_port = 16061;
    std::string model_path = "/models/phi-3-mini.q4.gguf";
    std::string log_level = "info";

    int context_size = 4096;
    // Donanım thread sayısının yarısı, max 8. Hyper-threading'i hesaba katarak.
    int n_threads = std::max(1u, std::min(8u, std::thread::hardware_concurrency() / 2));
    
    // Varsayılan sampling parametreleri
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
    
    s.http_port = get_env_var_as_int("LLM_HTTP_PORT", s.http_port);
    s.grpc_port = get_env_var_as_int("LLM_GRPC_PORT", s.grpc_port);
    s.model_path = get_env_var("LLM_MODEL_PATH", s.model_path);
    s.log_level = get_env_var("LOG_LEVEL", s.log_level);
    s.context_size = get_env_var_as_int("LLM_CONTEXT_SIZE", s.context_size);
    s.n_threads = get_env_var_as_int("LLM_THREADS", s.n_threads);
    return s;
}
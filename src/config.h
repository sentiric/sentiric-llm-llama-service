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
    std::string host = "0.0.0.0";
    int http_port = 16070;
    int grpc_port = 16071;

    std::string model_dir = "/models";
    std::string model_id = "";
    std::string model_filename = "";
    std::string model_path = "";
    std::string legacy_model_path = "";

    int n_gpu_layers = 0;
    uint32_t context_size = 4096;
    uint32_t n_threads = std::max(1u, std::min(8u, std::thread::hardware_concurrency() / 2));
    uint32_t n_threads_batch = std::max(1u, std::min(8u, std::thread::hardware_concurrency() / 2));
    uint32_t n_batch = 512;
    uint32_t n_ubatch = 512;
    ggml_numa_strategy numa_strategy = GGML_NUMA_STRATEGY_DISABLED;
    
    std::string log_level = "info";

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
    
    s.host = get_env_var("LLM_LLAMA_SERVICE_LISTEN_ADDRESS", s.host);
    s.http_port = get_env_var_as_int("LLM_LLAMA_SERVICE_HTTP_PORT", s.http_port);
    s.grpc_port = get_env_var_as_int("LLM_LLAMA_SERVICE_GRPC_PORT", s.grpc_port);
    
    s.model_dir = get_env_var("LLM_LLAMA_SERVICE_MODEL_DIR", s.model_dir);
    s.model_id = get_env_var("LLM_LLAMA_SERVICE_MODEL_ID", s.model_id);
    s.model_filename = get_env_var("LLM_LLAMA_SERVICE_MODEL_FILENAME", s.model_filename);
    
    s.n_gpu_layers = get_env_var_as_int("LLM_LLAMA_SERVICE_GPU_LAYERS", s.n_gpu_layers);
    s.context_size = get_env_var_as_uint("LLM_LLAMA_SERVICE_CONTEXT_SIZE", s.context_size);
    s.n_threads = get_env_var_as_uint("LLM_LLAMA_SERVICE_THREADS", s.n_threads);
    s.n_threads_batch = get_env_var_as_uint("LLM_LLAMA_SERVICE_THREADS_BATCH", s.n_threads_batch);
    s.n_batch = get_env_var_as_uint("LLM_LLAMA_SERVICE_BATCH_SIZE", s.n_batch);
    s.n_ubatch = get_env_var_as_uint("LLM_LLAMA_SERVICE_UBATCH_SIZE", s.n_ubatch);
    s.log_level = get_env_var("LLM_LLAMA_SERVICE_LOG_LEVEL", s.log_level);

    s.default_max_tokens = get_env_var_as_int("LLM_LLAMA_SERVICE_DEFAULT_MAX_TOKENS", s.default_max_tokens);
    s.default_temperature = get_env_var_as_float("LLM_LLAMA_SERVICE_DEFAULT_TEMPERATURE", s.default_temperature);
    s.default_top_k = get_env_var_as_int("LLM_LLAMA_SERVICE_DEFAULT_TOP_K", s.default_top_k);
    s.default_top_p = get_env_var_as_float("LLM_LLAMA_SERVICE_DEFAULT_TOP_P", s.default_top_p);
    s.default_repeat_penalty = get_env_var_as_float("LLM_LLAMA_SERVICE_DEFAULT_REPEAT_PENALTY", s.default_repeat_penalty);

    // Legacy path for backward compatibility
    s.legacy_model_path = get_env_var("LLM_LLAMA_SERVICE_MODEL_PATH", s.legacy_model_path);
    if (s.model_id.empty() && !s.legacy_model_path.empty()) {
        spdlog::warn("Using legacy LLM_LLAMA_SERVICE_MODEL_PATH. It's recommended to set MODEL_ID and MODEL_FILENAME instead.");
        std::filesystem::path p(s.legacy_model_path);
        s.model_filename = p.filename().string();
    }
    return s;
};
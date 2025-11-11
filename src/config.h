// src/config.h
// DÜZELTME: Eksik olan <filesystem> başlık dosyası eklendi.
#pragma once

#include <string>
#include <cstdlib>
#include <thread>
#include <stdexcept>
#include "spdlog/spdlog.h"
#include <algorithm>
#include <filesystem> // EKLENDİ: Dosya yolu manipülasyonu için gerekli

// filesystem namespace'ini header'da tanımlamak iyi bir pratik değildir.
// Bu yüzden inline fonksiyonda tam adını (std::filesystem) kullanacağız.

struct Settings {
    // Network Settings
    std::string host = "0.0.0.0";
    int http_port = 16070;
    int grpc_port = 16071;

    // Model & Engine Settings
    std::string model_dir = "/models";
    std::string model_id = "microsoft/Phi-3-mini-4k-instruct-gguf";
    std::string model_filename = "Phi-3-mini-4k-instruct-q4.gguf";
    std::string model_path = ""; // ModelManager tarafından doldurulacak olan nihai yol.
    std::string legacy_model_path = ""; // Eski sistemle uyumluluk için
    
    int context_size = 4096;
    int n_threads = std::max(1u, std::min(8u, std::thread::hardware_concurrency() / 2));
    
    int n_gpu_layers = 0; // GPU'ya yüklenecek katman sayısı

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
    
    s.host = get_env_var("LLM_LLAMA_SERVICE_IPV4_ADDRESS", s.host);
    s.http_port = get_env_var_as_int("LLM_LLAMA_SERVICE_HTTP_PORT", s.http_port);
    s.grpc_port = get_env_var_as_int("LLM_LLAMA_SERVICE_GRPC_PORT", s.grpc_port);
    
    s.model_dir = get_env_var("LLM_LLAMA_SERVICE_MODEL_DIR", s.model_dir);
    s.model_id = get_env_var("LLM_LLAMA_SERVICE_MODEL_ID", s.model_id);
    s.model_filename = get_env_var("LLM_LLAMA_SERVICE_MODEL_FILENAME", s.model_filename);
    s.legacy_model_path = get_env_var("LLM_LLAMA_SERVICE_MODEL_PATH", s.legacy_model_path);
    
    s.context_size = get_env_var_as_int("LLM_LLAMA_SERVICE_CONTEXT_SIZE", s.context_size);
    s.n_threads = get_env_var_as_int("LLM_LLAMA_SERVICE_THREADS", s.n_threads);
    s.log_level = get_env_var("LLM_LLAMA_SERVICE_LOG_LEVEL", s.log_level);
    
    s.n_gpu_layers = get_env_var_as_int("LLM_LLAMA_SERVICE_GPU_LAYERS", s.n_gpu_layers);
    
    // Geriye dönük uyumluluk: Eğer yeni MODEL_ID/FILENAME boşsa ama eski MODEL_PATH doluysa, ayarları buradan çıkar.
    if (s.model_id.empty() && !s.legacy_model_path.empty()) {
        spdlog::warn("Using legacy LLM_LLAMA_SERVICE_MODEL_PATH. It's recommended to set MODEL_ID and MODEL_FILENAME instead.");
        std::filesystem::path p(s.legacy_model_path);
        s.model_dir = p.parent_path().string();
        s.model_filename = p.filename().string();
        // ID'yi boş bırakarak ModelManager'ın eski davranışa dönmesini sağla.
    }

    return s;
}
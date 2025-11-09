#pragma once
#include <string>
#include <cstdlib>
#include <thread>

struct Settings {
    // Sunucu Ayarları
    int http_port = 16060;
    int grpc_port = 16061;
    std::string model_path = "/models/phi-3-mini.q4.gguf";
    std::string log_level = "info";

    // llama.cpp Ayarları
    int context_size = 4096;
    int n_threads = std::thread::hardware_concurrency();
    int n_batch = 512;

    // Sampling Parametreleri (Varsayılanlar)
    float default_temperature = 0.8f;
    int32_t default_top_k = 40;
    float default_top_p = 0.95f;
    float default_repeat_penalty = 1.1f;
    int32_t default_max_tokens = 2048;
    int32_t repeat_last_n = 64;
};

inline Settings load_settings() {
    Settings s;
    if (const char* env_p = std::getenv("LLM_LOCAL_SERVICE_HTTP_PORT")) s.http_port = std::stoi(env_p);
    if (const char* env_p = std::getenv("LLM_LOCAL_SERVICE_GRPC_PORT")) s.grpc_port = std::stoi(env_p);
    if (const char* env_p = std::getenv("LLM_LOCAL_SERVICE_MODEL_PATH")) s.model_path = env_p;
    if (const char* env_p = std::getenv("LOG_LEVEL")) s.log_level = env_p;
    if (const char* env_p = std::getenv("LLM_CONTEXT_SIZE")) s.context_size = std::stoi(env_p);
    if (const char* env_p = std::getenv("LLM_THREADS")) s.n_threads = std::stoi(env_p);
    
    return s;
}
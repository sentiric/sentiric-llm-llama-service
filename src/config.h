#pragma once
#include <string>
#include <cstdlib>
#include <spdlog/spdlog.h>
#include <thread>
#include <cmath> // std::fmax için eklendi

struct Settings {
    int http_port = 16060;
    int grpc_port = 16061;
    std::string model_path = "/models/phi-3-mini.q4.gguf";
    std::string log_level = "info";
    int context_size = 4096;
    int n_threads = std::thread::hardware_concurrency();
    int n_batch = 512;

    // Yeni Sampling Varsayılanları
    float default_temperature = 0.8f;
    int default_top_k = 40;
    float default_top_p = 0.95f;
};

inline Settings load_settings() {
    Settings s;
    if (const char* env_p = std::getenv("LLM_LOCAL_SERVICE_HTTP_PORT")) s.http_port = std::stoi(env_p);
    if (const char* env_p = std::getenv("LLM_LOCAL_SERVICE_GRPC_PORT")) s.grpc_port = std::stoi(env_p);
    if (const char* env_p = std::getenv("LLM_LOCAL_SERVICE_MODEL_PATH")) s.model_path = env_p;
    if (const char* env_p = std::getenv("LOG_LEVEL")) s.log_level = env_p;
    if (const char* env_p = std::getenv("LLM_CONTEXT_SIZE")) s.context_size = std::stoi(env_p);
    if (const char* env_p = std::getenv("LLM_THREADS")) s.n_threads = std::stoi(env_p);
    if (const char* env_p = std::getenv("LLM_BATCH_SIZE")) s.n_batch = std::stoi(env_p);
    
    // Gelişmiş sampling ayarları için Env değişkeni eklemedim, ancak istenseydi buraya eklenirdi.
    return s;
}
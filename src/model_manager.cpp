// src/model_manager.cpp
#include "model_manager.h"
#include "spdlog/spdlog.h"
#include <stdexcept>
#include <filesystem>
#include <cstdlib>
#include <fmt/core.h>

namespace fs = std::filesystem;

namespace ModelManager {

std::string ensure_model_is_ready(const Settings& settings) {
    if (settings.model_id.empty()) {
        // Eğer MODEL_ID belirtilmemişse, eski davranışa dön:
        // Modelin legacy_model_path'de manuel olarak bulunduğunu varsay.
        spdlog::warn("LLM_LLAMA_SERVICE_MODEL_ID not set. Assuming model exists at '{}' (from LLM_LLAMA_SERVICE_MODEL_PATH).", settings.legacy_model_path);
        if (!fs::exists(settings.legacy_model_path)) {
            throw std::runtime_error(fmt::format("Model file does not exist at the specified path: {}", settings.legacy_model_path));
        }
        return settings.legacy_model_path;
    }

    fs::path target_dir(settings.model_dir);
    fs::path target_filepath = target_dir / settings.model_filename;

    if (fs::exists(target_filepath)) {
        spdlog::info("Model file '{}' already exists. Skipping download.", target_filepath.string());
        return target_filepath.string();
    }

    spdlog::info("Model file '{}' not found. Starting download...", target_filepath.string());
    
    std::string url = fmt::format(
        "https://huggingface.co/{}/resolve/main/{}",
        settings.model_id,
        settings.model_filename
    );

    // İndirme komutunu oluştur (curl kullanarak)
    // -f: HTTP hatalarında hata koduyla çık
    // -L: Yönlendirmeleri takip et
    // -o: Çıktıyı dosyaya yaz
    std::string command = fmt::format(
        "curl -f -L \"{}\" -o \"{}\"",
        url,
        target_filepath.string()
    );

    spdlog::info("Executing download command: {}", command);

    if (!fs::exists(target_dir)) {
        spdlog::info("Creating model directory: {}", target_dir.string());
        fs::create_directories(target_dir);
    }
    
    int result = system(command.c_str());

    if (result != 0) {
        if (fs::exists(target_filepath)) {
            fs::remove(target_filepath);
        }
        throw std::runtime_error(fmt::format("Failed to download model from '{}'. Curl exited with code {}.", url, result));
    }
    
    spdlog::info("Model downloaded successfully to '{}'.", target_filepath.string());
    
    return target_filepath.string();
}

} // namespace ModelManager
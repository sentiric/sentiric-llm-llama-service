// src/model_manager.cpp
#include "model_manager.h"
#include "spdlog/spdlog.h"
#include <fmt/core.h>
#include <filesystem>
#include <cstdlib> // system() için

namespace fs = std::filesystem;

namespace ModelManager {

std::string ensure_model_is_ready(const Settings& settings) {
    if (settings.model_id.empty()) {
        spdlog::warn("LLM_LLAMA_SERVICE_MODEL_ID not set. Assuming model exists at '{}' (from LLM_LLAMA_SERVICE_MODEL_PATH).", settings.legacy_model_path);
        if (!fs::exists(settings.legacy_model_path)) {
            throw std::runtime_error(fmt::format("Model file does not exist at the specified path: {}", settings.legacy_model_path));
        }
        return settings.legacy_model_path;
    }

    fs::path target_dir(settings.model_dir);
    fs::path target_filepath = target_dir / settings.model_filename;

    if (fs::exists(target_filepath)) {
        try {
            if (fs::file_size(target_filepath) > 1024 * 1024) { // 1 MB'den büyükse geçerli
                spdlog::info("Model file '{}' already exists and seems valid. Skipping download.", target_filepath.string());
                return target_filepath.string();
            } else {
                spdlog::warn("Existing model file '{}' is too small. Deleting and re-downloading.", target_filepath.string());
                fs::remove(target_filepath);
            }
        } catch (...) { /* ignore */ }
    }

    // --- Sağlamlaştırılmış curl Komutu ile İndirme ---
    spdlog::info("Model file '{}' not found. Starting download via robust curl command...", target_filepath.string());

    std::string url = fmt::format(
        "https://huggingface.co/{}/resolve/main/{}",
        settings.model_id,
        settings.model_filename
    );
    
    fs::path temp_filepath = target_filepath.string() + ".tmp";

    // -f: HTTP hatalarında hata koduyla çık
    // -L: Yönlendirmeleri takip et
    // --create-dirs: Gerekirse dizinleri oluştur
    // -o: Çıktıyı geçici dosyaya yaz
    std::string command = fmt::format(
        "curl -fL --create-dirs \"{}\" -o \"{}\"",
        url,
        temp_filepath.string()
    );

    spdlog::info("Executing download command: {}", command);
    int result = system(command.c_str());

    if (result == 0 && fs::exists(temp_filepath) && fs::file_size(temp_filepath) > 1024 * 1024) {
        spdlog::info("✅ Model downloaded successfully. Renaming temporary file.");
        fs::rename(temp_filepath, target_filepath);
        return target_filepath.string();
    } else {
        spdlog::error("❌ Model download failed. Curl exited with code {}. Cleaning up temporary file.", result);
        if (fs::exists(temp_filepath)) {
            fs::remove(temp_filepath);
        }
        throw std::runtime_error(fmt::format("Failed to download model from '{}'.", url));
    }
}

} // namespace ModelManager
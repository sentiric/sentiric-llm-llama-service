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

    // 1. Hedef dosya var mı ve geçerli mi?
    if (fs::exists(target_filepath)) {
        try {
            if (fs::file_size(target_filepath) > 1024 * 1024) { // 1 MB'den büyükse geçerli kabul et
                spdlog::info("✅ Model file '{}' already exists and seems valid. Skipping download.", target_filepath.string());
                return target_filepath.string();
            } else {
                spdlog::warn("⚠️ Existing model file '{}' is too small/corrupt. Deleting and re-downloading.", target_filepath.string());
                fs::remove(target_filepath);
            }
        } catch (...) { /* ignore errors during check */ }
    }

    // --- SAĞLAMLAŞTIRILMIŞ İNDİRME MANTIĞI ---
    
    fs::path temp_filepath = target_filepath.string() + ".tmp";
    spdlog::info("⬇️ Starting robust download for '{}'...", settings.model_filename);

    std::string url = fmt::format(
        "https://huggingface.co/{}/resolve/main/{}",
        settings.model_id,
        settings.model_filename
    );

    // CURL FLAGLERİ:
    // -f: HTTP hatalarında (404, 500) sessizce fail ol (hata kodu döndür).
    // -L: Redirectleri takip et (HuggingFace redirect verir).
    // -C -: (RESUME) Eğer dosya varsa kaldığı yerden devam et.
    // --retry 10: Transient hatalarda (timeout, DNS) 10 kere dene.
    // --retry-delay 5: Denemeler arası 5 saniye bekle.
    // --connect-timeout 30: Bağlantı 30 saniyede kurulamazsa retry yap.
    // --keepalive-time 60: Uzun indirmelerde bağlantıyı canlı tut.
    // --create-dirs: Klasör yoksa oluştur.
    std::string command = fmt::format(
        "curl -fL -C - --retry 10 --retry-delay 5 --retry-max-time 600 --connect-timeout 30 --keepalive-time 60 --create-dirs \"{}\" -o \"{}\"",
        url,
        temp_filepath.string()
    );

    spdlog::info("Executing: curl ... -o {}", temp_filepath.filename().string());
    
    int result = system(command.c_str());

    // Başarı Kontrolü
    if (result == 0 && fs::exists(temp_filepath)) {
        // İndirme bitti, boyut kontrolü yap
        if (fs::file_size(temp_filepath) > 1024 * 1024) {
            spdlog::info("✅ Download complete. Finalizing file...");
            fs::rename(temp_filepath, target_filepath);
            return target_filepath.string();
        } else {
            spdlog::error("❌ Download finished but file is too small (<1MB). Source might be invalid.");
            fs::remove(temp_filepath); // Bozuk dosyayı temizle
            throw std::runtime_error("Downloaded file invalid.");
        }
    } else {
        // HATA YÖNETİMİ
        // Resume mantığı için: Eğer dosya oluştuysa ve belirli bir boyuttaysa silme! 
        // Bir sonraki denemede kaldığı yerden devam etsin.
        
        bool keep_partial = false;
        if (fs::exists(temp_filepath)) {
            try {
                auto size = fs::file_size(temp_filepath);
                if (size > 10 * 1024 * 1024) { // 10MB'dan büyükse değerli veri vardır, sakla.
                    keep_partial = true;
                    spdlog::warn("⚠️ Download interrupted. Keeping partial file ({:.2f} MB) for resume on next attempt.", size / 1024.0 / 1024.0);
                }
            } catch (...) {}
        }

        if (!keep_partial && fs::exists(temp_filepath)) {
            spdlog::warn("⚠️ Download failed and partial file is tiny. Cleaning up.");
            fs::remove(temp_filepath);
        }

        spdlog::error("❌ Model download failed via curl (Exit Code: {}).", result);
        throw std::runtime_error(fmt::format("Failed to download model from '{}'. Check logs.", url));
    }
}

} // namespace ModelManager
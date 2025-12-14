#include "model_manager.h"
#include "spdlog/spdlog.h"
#include <fmt/core.h>
#include <filesystem>
#include <curl/curl.h>
#include <cstdio>

namespace fs = std::filesystem;

namespace ModelManager {

static size_t write_data(void* ptr, size_t size, size_t nmemb, FILE* stream) {
    size_t written = fwrite(ptr, size, nmemb, stream);
    return written;
}

static int progress_callback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    (void)ultotal; (void)ulnow;
    ProgressCallback* cb = static_cast<ProgressCallback*>(clientp);
    if (cb && *cb && dltotal > 0) {
        (*cb)(static_cast<double>(dltotal), static_cast<double>(dlnow));
    }
    return 0;
}

long long get_remote_file_size(const std::string& url) {
    CURL* curl = curl_easy_init();
    double file_size = -1.0;
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L); 
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        
        if (curl_easy_perform(curl) == CURLE_OK) {
            curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &file_size);
        }
        curl_easy_cleanup(curl);
    }
    return static_cast<long long>(file_size);
}

std::string ensure_model_is_ready(const Settings& settings, ProgressCallback progress_cb) {
    if (settings.model_id.empty()) {
        spdlog::warn("LLM_LLAMA_SERVICE_MODEL_ID not set. Checking legacy path: '{}'", settings.legacy_model_path);
        if (!fs::exists(settings.legacy_model_path)) {
            throw std::runtime_error(fmt::format("Model not found at: {}", settings.legacy_model_path));
        }
        return settings.legacy_model_path;
    }

    fs::path target_dir(settings.model_dir);
    if (!fs::exists(target_dir)) {
        fs::create_directories(target_dir);
    }
    
    fs::path target_filepath = target_dir / settings.model_filename;
    fs::path temp_filepath = target_filepath.string() + ".tmp";
    
    // [DEĞİŞİKLİK] URL şablonu kullanılıyor.
    std::string url = settings.model_url_template;
    
    // Basit string replace (fmt kütüphanesi named argument desteği C++17'de sınırlı olabilir, manual replace daha güvenli)
    auto replace_all = [](std::string& str, const std::string& from, const std::string& to) {
        size_t start_pos = 0;
        while((start_pos = str.find(from, start_pos)) != std::string::npos) {
            str.replace(start_pos, from.length(), to);
            start_pos += to.length();
        }
    };

    replace_all(url, "{model_id}", settings.model_id);
    replace_all(url, "{filename}", settings.model_filename);

    if (fs::exists(target_filepath)) {
        if (fs::file_size(target_filepath) > 1024 * 1024) {
            spdlog::info("✅ Model '{}' exists and is valid. Skipping download.", settings.model_filename);
            return target_filepath.string();
        }
        spdlog::warn("⚠️ Existing model file is corrupt/small. Redownloading.");
        fs::remove(target_filepath);
    }

    spdlog::info("⬇️ Starting secure native download for '{}'", settings.model_filename);
    spdlog::info("🔗 URL: {}", url);

    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize libcurl.");
    }

    long long resume_byte_pos = 0;
    if (fs::exists(temp_filepath)) {
        resume_byte_pos = fs::file_size(temp_filepath);
        spdlog::info("🔄 Resuming download from byte {}", resume_byte_pos);
    }

    FILE* fp = fopen(temp_filepath.string().c_str(), resume_byte_pos > 0 ? "ab" : "wb");
    if (!fp) {
        curl_easy_cleanup(curl);
        throw std::runtime_error("Failed to open temp file for writing.");
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L); 
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 60L); 
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 30L); 

    if (resume_byte_pos > 0) {
        curl_easy_setopt(curl, CURLOPT_RESUME_FROM_LARGE, (curl_off_t)resume_byte_pos);
    }

    if (progress_cb) {
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progress_cb);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    }

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    
    fclose(fp);
    curl_easy_cleanup(curl);

    if (res == CURLE_OK) {
        if (http_code >= 200 && http_code < 300) {
            spdlog::info("✅ Download completed successfully.");
            fs::rename(temp_filepath, target_filepath);
            return target_filepath.string();
        } else {
             spdlog::error("❌ HTTP Error: {}", http_code);
             throw std::runtime_error(fmt::format("Download failed with HTTP {}", http_code));
        }
    } else {
        spdlog::error("❌ Curl Error: {}", curl_easy_strerror(res));
        if (res == CURLE_RANGE_ERROR) {
             spdlog::warn("⚠️ Range error detected. Assuming file is already complete.");
             fs::rename(temp_filepath, target_filepath);
             return target_filepath.string();
        }
        throw std::runtime_error(fmt::format("Download failed: {}", curl_easy_strerror(res)));
    }
}

} // namespace ModelManager
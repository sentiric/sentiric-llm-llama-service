#pragma once

#include "config.h"
#include <string>
#include <functional>

namespace ModelManager {
    
    // İlerleme durumunu bildirmek için callback tipi
    using ProgressCallback = std::function<void(double total, double now)>;

    // Verilen ayarlara göre modelin hazır olduğundan emin olur.
    // Native libcurl kullanarak güvenli indirme yapar.
    // Resume (kaldığı yerden devam) ve Timeout özelliklerine sahiptir.
    std::string ensure_model_is_ready(const Settings& settings, ProgressCallback progress_cb = nullptr);

    // Yardımcı: URL'den dosya boyutunu öğrenme (HEAD request)
    long long get_remote_file_size(const std::string& url);

} // namespace ModelManager
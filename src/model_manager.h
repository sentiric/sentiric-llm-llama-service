// src/model_manager.h
// Yeni model indirme ve doğrulama mantığını içerir.
#pragma once

#include "config.h"
#include <string>

namespace ModelManager {
    
    // Verilen ayarlara göre modelin hazır olduğundan emin olur.
    // Model yerel olarak mevcut değilse, onu indirir.
    // Başarılı olduğunda modelin tam yolunu döndürür.
    // Başarısızlık durumunda bir istisna fırlatır.
    std::string ensure_model_is_ready(const Settings& settings);

} // namespace ModelManager
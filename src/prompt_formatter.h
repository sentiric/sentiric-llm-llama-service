#pragma once

#include <string>

class PromptFormatter {
public:
    /**
     * @brief Verilen ham kullanıcı prompt'unu modelin anlayacağı
     *        sohbet şablonuna göre formatlar.
     * @param user_prompt Kullanıcıdan gelen orijinal metin.
     * @return Model için formatlanmış tam prompt metni.
     */
    static std::string format(const std::string& user_prompt);
};
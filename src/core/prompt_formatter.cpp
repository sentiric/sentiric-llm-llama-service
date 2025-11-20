// src/core/prompt_formatter.cpp
#include "core/prompt_formatter.h"
#include <sstream>
#include "spdlog/spdlog.h"
#include <stdexcept>
#include <algorithm>

// --- Gemma3Formatter Implementasyonu ---
// Referans: Gemma-2/3 Chat Template Standartları
std::string Gemma3Formatter::format(const sentiric::llm::v1::LLMLocalServiceGenerateStreamRequest& request) const {
    std::ostringstream oss;
    
    // 1. SYSTEM & RAG ENJEKSİYONU
    // Küçük modeller (2B/9B), System prompt'u ayrı bir turn yerine
    // ilk User turn'ünün içine gömüldüğünde daha iyi performans verir.
    
    std::string combined_system_context;
    
    // A. Kişilik ve Rol (System Prompt)
    if (!request.system_prompt().empty()) {
        combined_system_context += "GÖREV VE ROL:\n" + request.system_prompt() + "\n\n";
    }
    
    // B. Bilgi Bankası (RAG Context) - Examples klasöründeki veriler buraya gelir
    if (request.has_rag_context() && !request.rag_context().empty()) {
        combined_system_context += "BAĞLAM VE REFERANS BİLGİLER (Kesinlikle sadık kal):\n" 
                                   "--------------------------------------------------\n" 
                                   + request.rag_context() + 
                                   "\n--------------------------------------------------\n\n";
    }

    // 2. GEÇMİŞ (HISTORY) İŞLEME
    bool is_first_turn = true;
    
    for (const auto& turn : request.history()) {
        if (turn.role() == "user") {
            oss << "<start_of_turn>user\n";
            // İlk user mesajına sistem ve RAG bilgisini enjekte et
            if (is_first_turn && !combined_system_context.empty()) {
                oss << combined_system_context;
                combined_system_context.clear(); // Tekrar eklenmesin
                is_first_turn = false;
            }
            oss << turn.content() << "<end_of_turn>\n";
        } 
        else if (turn.role() == "assistant" || turn.role() == "model") {
            oss << "<start_of_turn>model\n" << turn.content() << "<end_of_turn>\n";
            is_first_turn = false;
        }
    }

    // 3. MEVCUT İSTEK (CURRENT TURN)
    oss << "<start_of_turn>user\n";
    
    // Eğer geçmiş boşsa ve hala sistem/rag basılmadıysa şimdi bas
    if (!combined_system_context.empty()) {
        oss << combined_system_context;
    }
    
    oss << request.user_prompt() << "<end_of_turn>\n";
    
    // 4. TETİKLEYİCİ (TRIGGER)
    oss << "<start_of_turn>model\n";
    
    return oss.str();
}

std::vector<std::string> Gemma3Formatter::get_stop_sequences() const {
    // Modelin durması gereken kesin noktalar.
    // Bunlar algılandığında üretim motor tarafından kesilir.
    return { 
        "<end_of_turn>", 
        "<eos>",
        "<start_of_turn>", // Model halüsinasyon görüp yeni turn başlatmaya kalkarsa durdur.
        "user:",           // Güvenlik ağı
        "model:"           // Güvenlik ağı
    };
}

std::unique_ptr<PromptFormatter> create_formatter_for_model(const std::string& model_architecture) {
    // İleride Llama-3, Mistral gibi modeller gelirse burada switch-case ile ayrıştıracağız.
    // Şu an Gemma standardını kullanıyoruz.
    (void)model_architecture; // Unused parameter warning fix
    return std::make_unique<Gemma3Formatter>();
}
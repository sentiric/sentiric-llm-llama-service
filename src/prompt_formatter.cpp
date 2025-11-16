#include "prompt_formatter.h"
#include <sstream>
#include <string>

// String içindeki bir alt metni başka bir metinle değiştiren yardımcı fonksiyon.
static void replace_all(std::string& str, const std::string& from, const std::string& to) {
    if (from.empty()) return;
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
}

std::string PromptFormatter::format(const sentiric::llm::v1::LLMLocalServiceGenerateStreamRequest& request) {
    std::ostringstream oss;

    // 1. Sistem Prompt'unu Hazırla
    std::string final_system_prompt = request.system_prompt();
    if (request.has_rag_context() && !request.rag_context().empty()) {
        replace_all(final_system_prompt, "{context}", request.rag_context());
        replace_all(final_system_prompt, "{query}", request.user_prompt());
    }
    oss << "<|system|>\n" << final_system_prompt << "<|end|>\n";

    // 2. Konuşma Geçmişini Ekle
    for (const auto& turn : request.history()) {
        if (turn.role() == "user") {
            oss << "<|user|>\n" << turn.content() << "<|end|>\n";
        } else if (turn.role() == "assistant") {
            oss << "<|assistant|>\n" << turn.content() << "<|end|>\n";
        }
    }

    // 3. Kullanıcının Son Mesajını Ekle (Eğer RAG değilse)
    // RAG senaryosunda user_prompt zaten system_prompt içine gömüldü.
    if (!request.has_rag_context() || request.rag_context().empty()) {
         oss << "<|user|>\n" << request.user_prompt() << "<|end|>\n";
    }

    // 4. Asistanın Cevap Başlangıcını Ekle (Modelin yazmaya başlaması için)
    oss << "<|assistant|>\n";
    
    return oss.str();
}
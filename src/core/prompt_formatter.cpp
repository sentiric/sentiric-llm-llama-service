#include "core/prompt_formatter.h"
#include "spdlog/spdlog.h"
#include <sstream>
#include <algorithm>

std::unique_ptr<PromptFormatter> create_formatter() {
    return std::make_unique<ExplicitChatMLFormatter>();
}

void replace_all(std::string& str, const std::string& from, const std::string& to) {
    if(from.empty()) return;
    size_t start_pos = 0;
    while((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
}

std::string ExplicitChatMLFormatter::format(const sentiric::llm::v1::GenerateStreamRequest& request, const llama_model* model, const Settings& settings) const {
    std::stringstream ss;

    // 1. SYSTEM IDENTITY (KİMLİK)
    // Modelin kim olduğunu ve nasıl davranması gerektiğini buraya koyuyoruz.
    std::string system_prompt = !request.system_prompt().empty() 
                                ? request.system_prompt() 
                                : settings.template_system_prompt;

    ss << "<|im_start|>system\n" << system_prompt << "<|im_end|>\n";

    // 2. CONVERSATION HISTORY (HAFIZA)
    for (const auto& turn : request.history()) {
        std::string role = (turn.role() == "user") ? "user" : "assistant";
        ss << "<|im_start|>" << role << "\n" << turn.content() << "<|im_end|>\n";
    }

    // 3. CURRENT TURN (ŞİMDİKİ SORU + RAG)
    ss << "<|im_start|>user\n";
    
    // Eğer RAG verisi varsa, bunu kullanıcının sorusuyla "Physical Binding" yapıyoruz.
    // Yani model soruyu okurken mecburen cevabı da okumuş oluyor.
    if (request.has_rag_context() && !request.rag_context().empty()) {
        std::string final_msg = settings.template_rag_prompt;
        
        // Şablondaki yer tutucuları doldur
        replace_all(final_msg, "{{rag_context}}", request.rag_context());
        replace_all(final_msg, "{{user_prompt}}", request.user_prompt());
        
        // Eğer şablonda user_prompt yoksa manuel ekle (Güvenlik)
        if (final_msg.find(request.user_prompt()) == std::string::npos) {
            final_msg += "\n\n" + request.user_prompt();
        }
        
        ss << final_msg;
    } else {
        ss << request.user_prompt();
    }
    
    ss << "<|im_end|>\n<|im_start|>assistant\n";

    return ss.str();
}
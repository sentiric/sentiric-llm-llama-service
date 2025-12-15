#include "core/prompt_formatter.h"
#include "spdlog/spdlog.h"
#include <sstream>
#include <algorithm>

std::unique_ptr<PromptFormatter> create_formatter() {
    // Production'da model metadata'sı riskli olabilir. 
    // Qwen 2.5 ve Llama 3 için Explicit Formatter kullanıyoruz.
    return std::make_unique<ExplicitChatMLFormatter>();
}

// Helper: String replace
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

    // 1. SYSTEM PROMPT İNŞASI
    std::string system_prompt = !request.system_prompt().empty() 
                                ? request.system_prompt() 
                                : settings.template_system_prompt;

    // ChatML Format: <|im_start|>system\n...<|im_end|>\n
    ss << "<|im_start|>system\n" << system_prompt << "<|im_end|>\n";

    // 2. GEÇMİŞ (HISTORY) İŞLEME
    for (const auto& turn : request.history()) {
        std::string role = (turn.role() == "user") ? "user" : "assistant";
        ss << "<|im_start|>" << role << "\n" << turn.content() << "<|im_end|>\n";
    }

    // 3. RAG CONTEXT VE USER PROMPT BİRLEŞTİRME (KRİTİK STRATEJİ)
    // RAG verisini System Prompt yerine User Prompt'un hemen üstüne koyuyoruz.
    // Bu, modelin dikkatini (attention mechanism) context'e daha çok vermesini sağlar.
    
    ss << "<|im_start|>user\n";
    
    if (request.has_rag_context() && !request.rag_context().empty()) {
        // Profildeki RAG şablonunu kullan
        std::string rag_block = settings.template_rag_prompt;
        
        // Yer tutucuları değiştir
        // Not: system_prompt burada tekrar kullanılmaz, yukarıda tanımlandı.
        replace_all(rag_block, "{{rag_context}}", request.rag_context());
        replace_all(rag_block, "{{user_prompt}}", request.user_prompt()); 
        
        // Eğer şablonda {{user_prompt}} yoksa, biz manuel ekleriz.
        if (rag_block.find(request.user_prompt()) == std::string::npos) {
             ss << rag_block << "\n\nSORU: " << request.user_prompt();
        } else {
             ss << rag_block;
        }
    } else {
        // RAG yoksa direkt soruyu bas
        ss << request.user_prompt();
    }
    
    ss << "<|im_end|>\n<|im_start|>assistant\n";

    return ss.str();
}
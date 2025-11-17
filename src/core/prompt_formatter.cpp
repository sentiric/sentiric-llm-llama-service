#include "core/prompt_formatter.h"
#include <sstream>
#include "spdlog/spdlog.h"

std::string PromptFormatter::format(const sentiric::llm::v1::LLMLocalServiceGenerateStreamRequest& request) {
    std::ostringstream oss;
    
    spdlog::debug("🔧 Prompt formatting - User: '{}', History: {}", 
                 request.user_prompt(), request.history_size());
    
    // BASİT FORMAT - Gemma 3 için
    // Sistem promptu
    if (!request.system_prompt().empty()) {
        oss << request.system_prompt() << "\n\n";
    }

    // RAG context
    if (request.has_rag_context() && !request.rag_context().empty()) {
        oss << "Context: " << request.rag_context() << "\n\n";
    }

    // Kullanıcı sorusu
    oss << "User: " << request.user_prompt() << "\n";
    
    // Assistant cevabı
    oss << "Assistant: ";
    
    std::string result = oss.str();
    spdlog::debug("🔧 SIMPLE Formatted prompt ({} chars): {}", result.length(), result.substr(0, 100) + "...");
    
    return result;
}

std::vector<std::string> PromptFormatter::get_stop_sequences() {
    // Basit stop sequences
    return { "\nUser:", "User:", "\n\n" };
}
#include "core/prompt_formatter.h"
#include <sstream>

// Gemma "instruct" modelleri için resmi sohbet şablonunu uygular.
std::string PromptFormatter::format(const sentiric::llm::v1::LLMLocalServiceGenerateStreamRequest& request) {
    std::ostringstream oss;
    
    for (const auto& turn : request.history()) {
        if (turn.role() == "user") {
            oss << "<start_of_turn>user\n" << turn.content() << "<end_of_turn>\n";
        } else if (turn.role() == "assistant" || turn.role() == "model") {
            oss << "<start_of_turn>model\n" << turn.content() << "<end_of_turn>\n";
        }
    }
    
    oss << "<start_of_turn>user\n";
    if (!request.system_prompt().empty()) { oss << request.system_prompt() << "\n\n"; }
    if (request.has_rag_context() && !request.rag_context().empty()) { oss << "### İlgili Bilgiler:\n" << request.rag_context() << "\n\n"; }
    oss << "### Soru:\n" << request.user_prompt() << "<end_of_turn>\n";
    oss << "<start_of_turn>model\n";
    
    return oss.str();
}

std::vector<std::string> PromptFormatter::get_stop_sequences() {
    return { "<end_of_turn>" };
}
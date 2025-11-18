// src/core/prompt_formatter.cpp
#include "core/prompt_formatter.h"
#include <sstream>
#include "spdlog/spdlog.h"
#include <stdexcept>

// --- Gemma3Formatter Implementasyonu ---
std::string Gemma3Formatter::format(const sentiric::llm::v1::LLMLocalServiceGenerateStreamRequest& request) const {
    std::ostringstream oss;
    
    // Gemma 3 modeli için özel formatlama
    // <start_of_turn>user\n{system_prompt}\n{rag_context}\n{user_prompt}<end_of_turn>\n<start_of_turn>model
    
    oss << "<start_of_turn>user\n";

    if (!request.system_prompt().empty()) {
        oss << request.system_prompt() << "\n\n";
    }

    if (request.has_rag_context() && !request.rag_context().empty()) {
        oss << "### İlgili Bilgiler:\n" << request.rag_context() << "\n\n";
    }

    // TODO: Konuşma geçmişi (history) formatlaması buraya eklenebilir.

    oss << "### Kullanıcının Sorusu:\n" << request.user_prompt() << "<end_of_turn>\n";
    oss << "<start_of_turn>model\n";
    
    std::string result = oss.str();
    spdlog::debug("🔧 [Gemma3Formatter] Formatted prompt ({} chars): {}", result.length(), result.substr(0, 200) + "...");
    
    return result;
}

std::vector<std::string> Gemma3Formatter::get_stop_sequences() const {
    // Gemma 3'ün özel bitiş jetonu
    return { "<end_of_turn>" };
}


// --- Fabrika Fonksiyonu Implementasyonu ---
std::unique_ptr<PromptFormatter> create_formatter_for_model(const std::string& model_architecture) {
    spdlog::info("Selecting prompt formatter for model architecture: '{}'", model_architecture);

    if (model_architecture == "gemma3") {
        return std::make_unique<Gemma3Formatter>();
    }
    // Gelecekte buraya başka modeller eklenebilir.
    // else if (model_architecture == "llama3") {
    //     return std::make_unique<Llama3Formatter>();
    // }
    
    spdlog::warn("No specific prompt formatter found for architecture '{}'. Falling back to Gemma3Formatter.", model_architecture);
    // Varsayılan olarak Gemma3'ü döndür, ancak bir uyarı ver.
    return std::make_unique<Gemma3Formatter>();
}
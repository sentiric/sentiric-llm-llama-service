// src/core/prompt_formatter.cpp
#include "core/prompt_formatter.h"
#include <sstream>
#include "spdlog/spdlog.h"
#include <stdexcept>

// --- Gemma3Formatter Implementasyonu ---
std::string Gemma3Formatter::format(const sentiric::llm::v1::LLMLocalServiceGenerateStreamRequest& request) const {
    std::ostringstream oss;
    
    // Sistem prompt'u, ilk kullanıcı mesajının bir parçası olarak formatlanır.
    std::string system_prompt_str;
    if (!request.system_prompt().empty()) {
        system_prompt_str = request.system_prompt() + "\n\n";
    }
    bool system_prompt_handled = false;

    // --- YENİ: Konuşma geçmişini işle ---
    for (const auto& turn : request.history()) {
        if (turn.role() == "user") {
            oss << "<start_of_turn>user\n";
            // Sistem prompt'unu sadece ilk kullanıcı mesajına ekle
            if (!system_prompt_handled && !system_prompt_str.empty()) {
                oss << system_prompt_str;
                system_prompt_handled = true;
            }
            oss << turn.content() << "<end_of_turn>\n";
        } else if (turn.role() == "assistant" || turn.role() == "model") {
            oss << "<start_of_turn>model\n" << turn.content() << "<end_of_turn>\n";
        }
    }

    // Mevcut kullanıcı isteğini formatla
    oss << "<start_of_turn>user\n";
    if (!system_prompt_handled && !system_prompt_str.empty()) {
        oss << system_prompt_str;
        system_prompt_handled = true;
    }

    if (request.has_rag_context() && !request.rag_context().empty()) {
        oss << "Verilen bilgileri kullanarak cevap ver:\n---BAĞLAM---\n" << request.rag_context() << "\n---BAĞLAM SONU---\n\n";
    }

    oss << request.user_prompt() << "<end_of_turn>\n";
    
    // Modelin cevap vermesi için son sinyali ekle
    oss << "<start_of_turn>model\n";
    
    std::string result = oss.str();
    spdlog::debug("🔧 [Gemma3Formatter] Formatted prompt with history ({} chars)", result.length());
    
    return result;
}

std::vector<std::string> Gemma3Formatter::get_stop_sequences() const {
    // Gemma'nın özel bitiş jetonları
    return { "<end_of_turn>", "<start_of_turn>" };
}


// --- Fabrika Fonksiyonu Implementasyonu ---
std::unique_ptr<PromptFormatter> create_formatter_for_model(const std::string& model_architecture) {
    spdlog::info("Selecting prompt formatter for model architecture: '{}'", model_architecture);

    if (model_architecture == "gemma3") {
        return std::make_unique<Gemma3Formatter>();
    }
    
    spdlog::warn("No specific prompt formatter found for architecture '{}'. Falling back to Gemma3Formatter.", model_architecture);
    return std::make_unique<Gemma3Formatter>();
}
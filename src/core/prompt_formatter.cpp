// src/core/prompt_formatter.cpp
#include "core/prompt_formatter.h"
#include <sstream>
#include "spdlog/spdlog.h"
#include <stdexcept>
#include <algorithm>

// --- Gemma3Formatter Implementasyonu ---
// Gemma v2/v3 Prompt Format:
// <start_of_turn>user\n{content}<end_of_turn>\n<start_of_turn>model\n
std::string Gemma3Formatter::format(const sentiric::llm::v1::LLMLocalServiceGenerateStreamRequest& request) const {
    std::ostringstream oss;
    
    // 1. Sistem Prompt (Gemma sistem promptlarını genellikle user turn içinde daha iyi anlar ama
    // resmi formatta system turn de destekleniyor olabilir. En güvenlisi user içine gömmektir.)
    
    bool has_system = !request.system_prompt().empty();
    bool has_rag = request.has_rag_context() && !request.rag_context().empty();

    // --- Geçmiş ---
    for (const auto& turn : request.history()) {
        if (turn.role() == "user") {
            oss << "<start_of_turn>user\n" << turn.content() << "<end_of_turn>\n";
        } else if (turn.role() == "assistant" || turn.role() == "model") {
            oss << "<start_of_turn>model\n" << turn.content() << "<end_of_turn>\n";
        }
    }

    // --- Mevcut Turn (User) ---
    oss << "<start_of_turn>user\n";
    
    if (has_system) {
        oss << "Talimatlar: " << request.system_prompt() << "\n\n";
    }
    
    if (has_rag) {
        oss << "Bağlam Bilgisi:\n" << request.rag_context() << "\n\n";
    }
    
    oss << request.user_prompt() << "<end_of_turn>\n";
    
    // --- Model Trigger ---
    // Gemma için burası kritiktir. <start_of_turn>model\n dedikten sonra
    // modelin devam etmesi beklenir.
    oss << "<start_of_turn>model\n";
    
    return oss.str();
}

std::vector<std::string> Gemma3Formatter::get_stop_sequences() const {
    return { "<end_of_turn>", "<eos>" };
}

std::unique_ptr<PromptFormatter> create_formatter_for_model(const std::string& model_architecture) {
    // Model ne olursa olsun şimdilik Gemma formatını zorluyoruz çünkü projedeki model bu.
    return std::make_unique<Gemma3Formatter>();
}
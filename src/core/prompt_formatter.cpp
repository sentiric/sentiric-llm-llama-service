#include "core/prompt_formatter.h"
#include "spdlog/spdlog.h"
#include <sstream>
#include <algorithm>

// Factory Implementation
std::unique_ptr<PromptFormatter> create_formatter(const std::string& model_id) {
    std::string id_lower = model_id;
    std::transform(id_lower.begin(), id_lower.end(), id_lower.begin(), ::tolower);

    if (id_lower.find("llama-3") != std::string::npos || id_lower.find("llama3") != std::string::npos) {
        spdlog::info("🎨 Formatter Selected: Llama 3 (Instruct)");
        return std::make_unique<Llama3Formatter>();
    } 
    else if (id_lower.find("gemma") != std::string::npos) {
        spdlog::info("🎨 Formatter Selected: Gemma (Instruct)");
        return std::make_unique<GemmaFormatter>();
    }
    else if (id_lower.find("qwen") != std::string::npos) {
        spdlog::info("🎨 Formatter Selected: Qwen (ChatML)");
        return std::make_unique<QwenChatMLFormatter>();
    }
    
    spdlog::warn("⚠️ Unknown model family '{}'. Falling back to Raw/Template formatter.", model_id);
    return std::make_unique<RawTemplateFormatter>();
}

// --- Common Helper ---
// (Moved to header/static but kept helper logic here if needed or inline)

// --- 1. Qwen ChatML ---
std::string QwenChatMLFormatter::format(const sentiric::llm::v1::GenerateStreamRequest& request, const llama_model* model, const Settings& settings) const {
    std::stringstream ss;
    
    // System
    std::string system_prompt = !request.system_prompt().empty() ? request.system_prompt() : settings.template_system_prompt;
    ss << "<|im_start|>system\n" << system_prompt << "<|im_end|>\n";

    // History
    for (const auto& turn : request.history()) {
        std::string role = (turn.role() == "user") ? "user" : "assistant";
        ss << "<|im_start|>" << role << "\n" << turn.content() << "<|im_end|>\n";
    }

    // User + RAG
    ss << "<|im_start|>user\n";
    ss << "/no_think\n"; // Qwen 3 specific optimization

    if (request.has_rag_context() && !request.rag_context().empty()) {
        std::string content = settings.template_rag_prompt;
        PromptFormatter::replace_all(content, "{{rag_context}}", request.rag_context());
        PromptFormatter::replace_all(content, "{{user_prompt}}", request.user_prompt());
        // Fallback for missing placeholder
        if (content.find(request.user_prompt()) == std::string::npos) {
            content += "\n\nQUERY: " + request.user_prompt();
        }
        ss << content;
    } else {
        ss << request.user_prompt();
    }
    
    ss << "<|im_end|>\n<|im_start|>assistant\n";
    return ss.str();
}

// --- 2. Llama 3 ---
std::string Llama3Formatter::format(const sentiric::llm::v1::GenerateStreamRequest& request, const llama_model* model, const Settings& settings) const {
    std::stringstream ss;
    
    // System: <|start_header_id|>system<|end_header_id|>\n\n...<|eot_id|>
    std::string system_prompt = !request.system_prompt().empty() ? request.system_prompt() : settings.template_system_prompt;
    ss << "<|start_header_id|>system<|end_header_id|>\n\n" << system_prompt << "<|eot_id|>";

    // History
    for (const auto& turn : request.history()) {
        std::string role = (turn.role() == "user") ? "user" : "assistant";
        ss << "<|start_header_id|>" << role << "<|end_header_id|>\n\n" << turn.content() << "<|eot_id|>";
    }

    // User
    ss << "<|start_header_id|>user<|end_header_id|>\n\n";
    if (request.has_rag_context() && !request.rag_context().empty()) {
        std::string content = settings.template_rag_prompt;
        PromptFormatter::replace_all(content, "{{rag_context}}", request.rag_context());
        PromptFormatter::replace_all(content, "{{user_prompt}}", request.user_prompt());
        ss << content;
    } else {
        ss << request.user_prompt();
    }
    ss << "<|eot_id|>";

    // Generation Prompt
    ss << "<|start_header_id|>assistant<|end_header_id|>\n\n";
    return ss.str();
}

// --- 3. Gemma ---
std::string GemmaFormatter::format(const sentiric::llm::v1::GenerateStreamRequest& request, const llama_model* model, const Settings& settings) const {
    std::stringstream ss;
    
    // System (Gemma often puts system inside user turn or implicit)
    // But official instruct format: <start_of_turn>user\n...<end_of_turn>
    std::string system_prompt = !request.system_prompt().empty() ? request.system_prompt() : settings.template_system_prompt;
    
    // History (Assuming history includes system logic if needed, or prepending system to first user msg)
    // Here we treat system as a context block for the first user message if separate system role isn't strictly supported by tokenizer
    // But for consistency:
    
    bool first_msg = true;

    for (const auto& turn : request.history()) {
        std::string role = (turn.role() == "user") ? "user" : "model";
        ss << "<start_of_turn>" << role << "\n";
        if (first_msg && role == "user") {
            ss << "System: " << system_prompt << "\n\n";
            first_msg = false;
        }
        ss << turn.content() << "<end_of_turn>\n";
    }

    // Current Turn
    ss << "<start_of_turn>user\n";
    if (first_msg) { // No history
         ss << "System: " << system_prompt << "\n\n";
    }

    if (request.has_rag_context() && !request.rag_context().empty()) {
        std::string content = settings.template_rag_prompt;
        PromptFormatter::replace_all(content, "{{rag_context}}", request.rag_context());
        PromptFormatter::replace_all(content, "{{user_prompt}}", request.user_prompt());
        ss << content;
    } else {
        ss << request.user_prompt();
    }
    ss << "<end_of_turn>\n<start_of_turn>model\n";
    
    return ss.str();
}

// --- 4. Raw/Template (Fallback) ---
std::string RawTemplateFormatter::format(const sentiric::llm::v1::GenerateStreamRequest& request, const llama_model* model, const Settings& settings) const {
    std::stringstream ss;
    // Fallback: Just dump system + user. Very basic.
    std::string system_prompt = !request.system_prompt().empty() ? request.system_prompt() : settings.template_system_prompt;
    
    ss << "System: " << system_prompt << "\n\n";
    
    for (const auto& turn : request.history()) {
        ss << turn.role() << ": " << turn.content() << "\n\n";
    }
    
    ss << "User: ";
    if (request.has_rag_context() && !request.rag_context().empty()) {
         ss << request.rag_context() << "\nQuestion: " << request.user_prompt();
    } else {
         ss << request.user_prompt();
    }
    ss << "\nAssistant:";
    return ss.str();
}
#include "core/prompt_formatter.h"
#include "spdlog/spdlog.h"
#include <sstream>
#include <algorithm>

// Factory Implementation
std::unique_ptr<PromptFormatter> create_formatter(const std::string& model_id) {
    std::string id_lower = model_id;
    std::transform(id_lower.begin(), id_lower.end(), id_lower.begin(), ::tolower);

    // Ministral / Mistral Ailesi
    if (id_lower.find("ministral") != std::string::npos || id_lower.find("mistral") != std::string::npos) {
        spdlog::info("🎨 Formatter Selected: Mistral/Ministral (Inst)");
        return std::make_unique<MistralFormatter>();
    }
    else if (id_lower.find("llama-3") != std::string::npos || id_lower.find("llama3") != std::string::npos) {
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
    
    if (request.has_rag_context() && !request.rag_context().empty()) {
        std::string content = settings.template_rag_prompt;
        PromptFormatter::replace_all(content, "{{rag_context}}", request.rag_context());
        PromptFormatter::replace_all(content, "{{user_prompt}}", request.user_prompt());
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
    
    std::string system_prompt = !request.system_prompt().empty() ? request.system_prompt() : settings.template_system_prompt;
    ss << "<|start_header_id|>system<|end_header_id|>\n\n" << system_prompt << "<|eot_id|>";

    for (const auto& turn : request.history()) {
        std::string role = (turn.role() == "user") ? "user" : "assistant";
        ss << "<|start_header_id|>" << role << "<|end_header_id|>\n\n" << turn.content() << "<|eot_id|>";
    }

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
    ss << "<|start_header_id|>assistant<|end_header_id|>\n\n";
    return ss.str();
}

// --- 3. Mistral / Ministral (YENİ) ---
// Format: <s>[INST] System + User [/INST] Model </s> [INST] User [/INST] ...
std::string MistralFormatter::format(const sentiric::llm::v1::GenerateStreamRequest& request, const llama_model* model, const Settings& settings) const {
    std::stringstream ss;
    std::string system_prompt = !request.system_prompt().empty() ? request.system_prompt() : settings.template_system_prompt;
    
    // Mistral usually combines System into the first User instruction
    bool first = true;
    
    for (const auto& turn : request.history()) {
        if (turn.role() == "user") {
            ss << "[INST] ";
            if (first) {
                ss << system_prompt << "\n\n";
                first = false;
            }
            ss << turn.content() << " [/INST]";
        } else {
            ss << " " << turn.content() << "</s> ";
        }
    }
    
    ss << "[INST] ";
    if (first) { // No history
        ss << system_prompt << "\n\n";
    }

    if (request.has_rag_context() && !request.rag_context().empty()) {
        std::string content = settings.template_rag_prompt;
        PromptFormatter::replace_all(content, "{{rag_context}}", request.rag_context());
        PromptFormatter::replace_all(content, "{{user_prompt}}", request.user_prompt());
        ss << content;
    } else {
        ss << request.user_prompt();
    }
    ss << " [/INST]";

    return ss.str();
}

// --- 4. Gemma ---
std::string GemmaFormatter::format(const sentiric::llm::v1::GenerateStreamRequest& request, const llama_model* model, const Settings& settings) const {
    std::stringstream ss;
    
    std::string system_prompt = !request.system_prompt().empty() ? request.system_prompt() : settings.template_system_prompt;
    
    // Gemma doesn't strictly have a system role in official template, usually mapped to user turn
    // <start_of_turn>user\nSystem info...\nUser msg<end_of_turn><start_of_turn>model\n
    
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

    ss << "<start_of_turn>user\n";
    if (first_msg) { 
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

// --- 5. Raw/Template ---
std::string RawTemplateFormatter::format(const sentiric::llm::v1::GenerateStreamRequest& request, const llama_model* model, const Settings& settings) const {
    std::stringstream ss;
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
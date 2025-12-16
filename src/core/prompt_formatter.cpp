#include "core/prompt_formatter.h"
#include "spdlog/spdlog.h"
#include <sstream>
#include <algorithm>

// Factory
std::unique_ptr<PromptFormatter> create_formatter(const std::string& model_id) {
    std::string id_lower = model_id;
    std::transform(id_lower.begin(), id_lower.end(), id_lower.begin(), ::tolower);

    if (id_lower.find("ministral") != std::string::npos || id_lower.find("mistral") != std::string::npos) {
        return std::make_unique<MistralFormatter>();
    }
    else if (id_lower.find("llama-3") != std::string::npos || id_lower.find("llama3") != std::string::npos) {
        return std::make_unique<Llama3Formatter>();
    } 
    else if (id_lower.find("gemma") != std::string::npos) {
        return std::make_unique<GemmaFormatter>();
    }
    else if (id_lower.find("qwen") != std::string::npos) {
        return std::make_unique<QwenChatMLFormatter>();
    }
    return std::make_unique<RawTemplateFormatter>();
}

// --- Helper ---
std::string build_final_user_content(const Settings& settings, const std::string& rag_context, const std::string& user_prompt) {
    std::string content = settings.template_rag_prompt;
    if (content.empty()) content = "CONTEXT:\n{{rag_context}}\n\nQUERY:\n{{user_prompt}}";
    
    PromptFormatter::replace_all(content, "{{rag_context}}", rag_context);
    PromptFormatter::replace_all(content, "{{user_prompt}}", user_prompt);
    
    // Güvenlik: Eğer template bozuksa ve placeholderlar silinmediyse, manuel ekle
    if (content.find(user_prompt) == std::string::npos) {
        content = "Context:\n" + rag_context + "\n\nQuestion: " + user_prompt;
    }
    return content;
}

// --- 1. Qwen ChatML ---
std::string QwenChatMLFormatter::format(const sentiric::llm::v1::GenerateStreamRequest& request, const llama_model* model, const Settings& settings) const {
    std::stringstream ss;
    std::string system_prompt = !request.system_prompt().empty() ? request.system_prompt() : settings.template_system_prompt;
    
    // System
    ss << "<|im_start|>system\n" << system_prompt << "<|im_end|>\n";

    // History
    for (const auto& turn : request.history()) {
        std::string role = (turn.role() == "user") ? "user" : "assistant";
        ss << "<|im_start|>" << role << "\n" << turn.content() << "<|im_end|>\n";
    }

    // User Turn (RAG burada birleşir)
    ss << "<|im_start|>user\n";
    if (request.has_rag_context() && !request.rag_context().empty()) {
        ss << build_final_user_content(settings, request.rag_context(), request.user_prompt());
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
        ss << build_final_user_content(settings, request.rag_context(), request.user_prompt());
    } else {
        ss << request.user_prompt();
    }
    ss << "<|eot_id|><|start_header_id|>assistant<|end_header_id|>\n\n";
    return ss.str();
}

// --- 3. Mistral / Ministral ---
std::string MistralFormatter::format(const sentiric::llm::v1::GenerateStreamRequest& request, const llama_model* model, const Settings& settings) const {
    std::stringstream ss;
    std::string system_prompt = !request.system_prompt().empty() ? request.system_prompt() : settings.template_system_prompt;
    
    // Ministral için System prompt'u User mesajının en başına ekliyoruz.
    // [INST] System + Context + Question [/INST]
    
    ss << "[INST] ";
    
    // System Prompt (Varsa sadece en başa)
    if (request.history().empty()) {
        ss << system_prompt << "\n\n";
    }

    // History
    for (const auto& turn : request.history()) {
        if (turn.role() == "user") {
            // History içindeki user mesajları
            ss << turn.content() << " [/INST]";
        } else {
            ss << " " << turn.content() << "</s> [INST] ";
        }
    }

    // Current User Turn (RAG Dahil)
    if (request.has_rag_context() && !request.rag_context().empty()) {
        ss << build_final_user_content(settings, request.rag_context(), request.user_prompt());
    } else {
        ss << request.user_prompt();
    }
    ss << " [/INST]";
    
    return ss.str();
}

// --- 4. Gemma (Strict Fix) ---
std::string GemmaFormatter::format(const sentiric::llm::v1::GenerateStreamRequest& request, const llama_model* model, const Settings& settings) const {
    std::stringstream ss;
    std::string system_prompt = !request.system_prompt().empty() ? request.system_prompt() : settings.template_system_prompt;

    // Gemma System Prompt desteklemez. <start_of_turn>user bloğunun en başında düz metin olarak verilmelidir.
    
    // History Processing
    bool is_first = true;
    for (const auto& turn : request.history()) {
        std::string role = (turn.role() == "user") ? "user" : "model";
        ss << "<start_of_turn>" << role << "\n";
        
        // System prompt'u ilk user mesajının başına göm
        if (is_first && role == "user") {
            ss << system_prompt << "\n\n";
            is_first = false;
        }
        
        ss << turn.content() << "<end_of_turn>\n";
    }

    // Current Turn
    ss << "<start_of_turn>user\n";
    
    if (is_first) { // Eğer history yoksa, system prompt buraya gelir
        ss << system_prompt << "\n\n";
    }

    if (request.has_rag_context() && !request.rag_context().empty()) {
        ss << build_final_user_content(settings, request.rag_context(), request.user_prompt());
    } else {
        ss << request.user_prompt();
    }
    
    ss << "<end_of_turn>\n<start_of_turn>model\n";
    return ss.str();
}

// --- 5. Raw ---
std::string RawTemplateFormatter::format(const sentiric::llm::v1::GenerateStreamRequest& request, const llama_model* model, const Settings& settings) const {
    std::stringstream ss;
    std::string system_prompt = !request.system_prompt().empty() ? request.system_prompt() : settings.template_system_prompt;
    
    ss << system_prompt << "\n\n";
    for (const auto& turn : request.history()) {
        ss << turn.role() << ": " << turn.content() << "\n";
    }
    
    ss << "User: ";
    if (request.has_rag_context() && !request.rag_context().empty()) {
         ss << build_final_user_content(settings, request.rag_context(), request.user_prompt());
    } else {
         ss << request.user_prompt();
    }
    ss << "\nAssistant:";
    return ss.str();
}
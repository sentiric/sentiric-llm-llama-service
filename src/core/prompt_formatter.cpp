#include "core/prompt_formatter.h"
#include "spdlog/spdlog.h"
#include <sstream>
#include <algorithm>

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

// --- Helper: Basit ve Güçlü RAG Birleştirici ---
std::string combine_rag_and_query(const std::string& rag, const std::string& query) {
    if (rag.empty()) return query;
    // Veriyi sorunun hemen üstüne, çok belirgin şekilde koyuyoruz.
    return "VERİ:\n" + rag + "\n\nSORU:\n" + query;
}

// --- 1. Qwen ChatML ---
std::string QwenChatMLFormatter::format(const sentiric::llm::v1::GenerateStreamRequest& request, const llama_model* model, const Settings& settings) const {
    std::stringstream ss;
    // System
    std::string system = !request.system_prompt().empty() ? request.system_prompt() : settings.template_system_prompt;
    ss << "<|im_start|>system\n" << system << "<|im_end|>\n";

    // History
    for (const auto& turn : request.history()) {
        std::string role = (turn.role() == "user") ? "user" : "assistant";
        ss << "<|im_start|>" << role << "\n" << turn.content() << "<|im_end|>\n";
    }

    // User Turn
    std::string final_content = request.user_prompt();
    if (request.has_rag_context() && !request.rag_context().empty()) {
        final_content = combine_rag_and_query(request.rag_context(), request.user_prompt());
    }

    ss << "<|im_start|>user\n" << final_content << "<|im_end|>\n<|im_start|>assistant\n";
    return ss.str();
}

// --- 2. Llama 3 ---
std::string Llama3Formatter::format(const sentiric::llm::v1::GenerateStreamRequest& request, const llama_model* model, const Settings& settings) const {
    std::stringstream ss;
    std::string system = !request.system_prompt().empty() ? request.system_prompt() : settings.template_system_prompt;
    
    ss << "<|start_header_id|>system<|end_header_id|>\n\n" << system << "<|eot_id|>";

    for (const auto& turn : request.history()) {
        std::string role = (turn.role() == "user") ? "user" : "assistant";
        ss << "<|start_header_id|>" << role << "<|end_header_id|>\n\n" << turn.content() << "<|eot_id|>";
    }

    std::string final_content = request.user_prompt();
    if (request.has_rag_context() && !request.rag_context().empty()) {
        final_content = combine_rag_and_query(request.rag_context(), request.user_prompt());
    }

    ss << "<|start_header_id|>user<|end_header_id|>\n\n" << final_content << "<|eot_id|>";
    ss << "<|start_header_id|>assistant<|end_header_id|>\n\n";
    return ss.str();
}

// --- 3. Mistral / Ministral ---
std::string MistralFormatter::format(const sentiric::llm::v1::GenerateStreamRequest& request, const llama_model* model, const Settings& settings) const {
    std::stringstream ss;
    std::string system = !request.system_prompt().empty() ? request.system_prompt() : settings.template_system_prompt;
    
    // Mistral v3 için System promptu ayrı bir blok değil, instruction içine gömüyoruz.
    ss << "[INST] " << system << "\n\n";

    for (const auto& turn : request.history()) {
        if (turn.role() == "user") {
            ss << turn.content() << " [/INST]";
        } else {
            ss << " " << turn.content() << "</s> [INST] ";
        }
    }

    std::string final_content = request.user_prompt();
    if (request.has_rag_context() && !request.rag_context().empty()) {
        final_content = combine_rag_and_query(request.rag_context(), request.user_prompt());
    }

    ss << final_content << " [/INST]";
    return ss.str();
}

// --- 4. Gemma (Strict Fix) ---
std::string GemmaFormatter::format(const sentiric::llm::v1::GenerateStreamRequest& request, const llama_model* model, const Settings& settings) const {
    std::stringstream ss;
    std::string system = !request.system_prompt().empty() ? request.system_prompt() : settings.template_system_prompt;

    // Gemma için System Prompt'u User turn'ünün içine gömüyoruz.
    // ÖNEMLİ: <start_of_turn>model\n ile bitirmeliyiz.
    
    // History
    bool first = true;
    for (const auto& turn : request.history()) {
        std::string role = (turn.role() == "user") ? "user" : "model";
        ss << "<start_of_turn>" << role << "\n";
        
        if (first && role == "user") {
            ss << "Talimat: " << system << "\n\n";
            first = false;
        }
        
        ss << turn.content() << "<end_of_turn>\n";
    }

    // Current Turn
    ss << "<start_of_turn>user\n";
    if (first) {
        ss << "Talimat: " << system << "\n\n";
    }

    std::string final_content = request.user_prompt();
    if (request.has_rag_context() && !request.rag_context().empty()) {
        final_content = combine_rag_and_query(request.rag_context(), request.user_prompt());
    }
    
    ss << final_content << "<end_of_turn>\n<start_of_turn>model\n";
    return ss.str();
}

// --- 5. Raw ---
std::string RawTemplateFormatter::format(const sentiric::llm::v1::GenerateStreamRequest& request, const llama_model* model, const Settings& settings) const {
    std::stringstream ss;
    std::string system = !request.system_prompt().empty() ? request.system_prompt() : settings.template_system_prompt;
    
    ss << system << "\n\n";
    
    for (const auto& turn : request.history()) {
        ss << turn.role() << ": " << turn.content() << "\n";
    }
    
    std::string final_content = request.user_prompt();
    if (request.has_rag_context() && !request.rag_context().empty()) {
        final_content = combine_rag_and_query(request.rag_context(), request.user_prompt());
    }
    
    ss << "User: " << final_content << "\nAssistant:";
    return ss.str();
}
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

// --- GÜÇLÜ RAG BİRLEŞTİRİCİ ---
// Modellerin veriyi görmezden gelmesini engellemek için "BAĞLAM" bloğunu
// sorunun hemen tepesine koyuyoruz.
std::string inject_context(const std::string& rag, const std::string& query) {
    if (rag.empty()) return query;
    return "BELGE:\n" + rag + "\n\nSORU:\n" + query + "\n\nCEVAP (Sadece Belgeye Göre):";
}

// --- 1. Qwen ChatML ---
// <|im_start|>user\nCONTENT<|im_end|>\n<|im_start|>assistant\n
std::string QwenChatMLFormatter::format(const sentiric::llm::v1::GenerateStreamRequest& request, const llama_model* model, const Settings& settings) const {
    std::stringstream ss;
    
    // System: Qwen System Prompt'a saygı duyar.
    std::string sys = !request.system_prompt().empty() ? request.system_prompt() : settings.template_system_prompt;
    ss << "<|im_start|>system\n" << sys << "<|im_end|>\n";

    for (const auto& turn : request.history()) {
        std::string role = (turn.role() == "user") ? "user" : "assistant";
        ss << "<|im_start|>" << role << "\n" << turn.content() << "<|im_end|>\n";
    }

    std::string user_content = request.user_prompt();
    if (request.has_rag_context() && !request.rag_context().empty()) {
        user_content = inject_context(request.rag_context(), request.user_prompt());
    }

    ss << "<|im_start|>user\n" << user_content << "<|im_end|>\n<|im_start|>assistant\n";
    return ss.str();
}

// --- 2. Llama 3 ---
// <|start_header_id|>user<|end_header_id|>\n\nCONTENT<|eot_id|>...
std::string Llama3Formatter::format(const sentiric::llm::v1::GenerateStreamRequest& request, const llama_model* model, const Settings& settings) const {
    std::stringstream ss;
    std::string sys = !request.system_prompt().empty() ? request.system_prompt() : settings.template_system_prompt;
    
    ss << "<|start_header_id|>system<|end_header_id|>\n\n" << sys << "<|eot_id|>";

    for (const auto& turn : request.history()) {
        std::string role = (turn.role() == "user") ? "user" : "assistant";
        ss << "<|start_header_id|>" << role << "<|end_header_id|>\n\n" << turn.content() << "<|eot_id|>";
    }

    std::string user_content = request.user_prompt();
    if (request.has_rag_context() && !request.rag_context().empty()) {
        user_content = inject_context(request.rag_context(), request.user_prompt());
    }

    ss << "<|start_header_id|>user<|end_header_id|>\n\n" << user_content << "<|eot_id|>";
    ss << "<|start_header_id|>assistant<|end_header_id|>\n\n";
    return ss.str();
}

// --- 3. Mistral / Ministral ---
// [INST] System \n\n User [/INST]
std::string MistralFormatter::format(const sentiric::llm::v1::GenerateStreamRequest& request, const llama_model* model, const Settings& settings) const {
    std::stringstream ss;
    std::string sys = !request.system_prompt().empty() ? request.system_prompt() : settings.template_system_prompt;
    
    // Ministral için en güvenli yol: System prompt yokmuş gibi davranıp her şeyi User Instruction'a gömmek.
    
    ss << "[INST] ";
    bool first = true;
    
    // History varsa önce onları işle
    if (!request.history().empty()) {
        ss << sys << "\n\n"; // System'i en başa koy
        first = false; 
        for (const auto& turn : request.history()) {
            if (turn.role() == "user") {
                ss << turn.content() << " [/INST]";
            } else {
                ss << " " << turn.content() << "</s> [INST] ";
            }
        }
    } else {
        // History yoksa System'i buraya koyacağız
        ss << sys << "\n\n";
    }

    std::string user_content = request.user_prompt();
    if (request.has_rag_context() && !request.rag_context().empty()) {
        user_content = inject_context(request.rag_context(), request.user_prompt());
    }

    ss << user_content << " [/INST]";
    return ss.str();
}

// --- 4. Gemma (STRICT MODE) ---
// <start_of_turn>user\nCONTENT<end_of_turn>\n<start_of_turn>model\n
std::string GemmaFormatter::format(const sentiric::llm::v1::GenerateStreamRequest& request, const llama_model* model, const Settings& settings) const {
    std::stringstream ss;
    std::string sys = !request.system_prompt().empty() ? request.system_prompt() : settings.template_system_prompt;

    // Gemma System Prompt desteklemez. Bunu User inputunun başına ekliyoruz.
    
    bool first_msg = true;
    for (const auto& turn : request.history()) {
        std::string role = (turn.role() == "user") ? "user" : "model";
        ss << "<start_of_turn>" << role << "\n";
        
        if (first_msg && role == "user") {
            ss << "Role: " << sys << "\n\n";
            first_msg = false;
        }
        
        ss << turn.content() << "<end_of_turn>\n";
    }

    ss << "<start_of_turn>user\n";
    if (first_msg) {
        ss << "Role: " << sys << "\n\n";
    }

    std::string user_content = request.user_prompt();
    if (request.has_rag_context() && !request.rag_context().empty()) {
        user_content = inject_context(request.rag_context(), request.user_prompt());
    }
    
    ss << user_content << "<end_of_turn>\n<start_of_turn>model\n";
    return ss.str();
}

// --- 5. Raw ---
std::string RawTemplateFormatter::format(const sentiric::llm::v1::GenerateStreamRequest& request, const llama_model* model, const Settings& settings) const {
    std::stringstream ss;
    std::string sys = !request.system_prompt().empty() ? request.system_prompt() : settings.template_system_prompt;
    
    ss << "System: " << sys << "\n\n";
    for (const auto& turn : request.history()) {
        ss << turn.role() << ": " << turn.content() << "\n";
    }
    
    std::string user_content = request.user_prompt();
    if (request.has_rag_context() && !request.rag_context().empty()) {
        user_content = inject_context(request.rag_context(), request.user_prompt());
    }
    
    ss << "User: " << user_content << "\nAssistant:";
    return ss.str();
}
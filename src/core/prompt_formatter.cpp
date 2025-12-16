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

// YENİ YARDIMCI FONKSİYON: Sadece son kullanıcı girdisini RAG ile zenginleştirir.
std::string build_final_user_content(const sentiric::llm::v1::GenerateStreamRequest& request, const Settings& settings) {
    if (request.has_rag_context() && !request.rag_context().empty()) {
        std::string final_prompt = settings.template_rag_prompt;
        PromptFormatter::replace_all(final_prompt, "{{rag_context}}", request.rag_context());
        PromptFormatter::replace_all(final_prompt, "{{user_prompt}}", request.user_prompt());
        return final_prompt;
    }
    return request.user_prompt();
}

// --- 1. Qwen ChatML ---
std::string QwenChatMLFormatter::format(const sentiric::llm::v1::GenerateStreamRequest& request, const Settings& settings) const {
    std::stringstream ss;
    
    // [FIX] Qwen için System Prompt'u zorunlu olarak en başa ekle
    std::string sys = !request.system_prompt().empty() ? request.system_prompt() : settings.template_system_prompt;
    if (!sys.empty()) {
        ss << "<|im_start|>system\n" << sys << "<|im_end|>\n";
    }

    for (const auto& turn : request.history()) {
        std::string role = (turn.role() == "user") ? "user" : "assistant";
        ss << "<|im_start|>" << role << "\n" << turn.content() << "<|im_end|>\n";
    }

    std::string user_content = build_final_user_content(request, settings);

    ss << "<|im_start|>user\n" << user_content << "<|im_end|>\n<|im_start|>assistant\n";
    return ss.str();
}

// --- 2. Llama 3 ---
std::string Llama3Formatter::format(const sentiric::llm::v1::GenerateStreamRequest& request, const Settings& settings) const {
    std::stringstream ss;
    std::string sys = !request.system_prompt().empty() ? request.system_prompt() : settings.template_system_prompt;
    
    ss << "<|start_header_id|>system<|end_header_id|>\n\n" << sys << "<|eot_id|>";

    for (const auto& turn : request.history()) {
        std::string role = (turn.role() == "user") ? "user" : "assistant";
        ss << "<|start_header_id|>" << role << "<|end_header_id|>\n\n" << turn.content() << "<|eot_id|>";
    }

    std::string user_content = build_final_user_content(request, settings);

    ss << "<|start_header_id|>user<|end_header_id|>\n\n" << user_content << "<|eot_id|>";
    ss << "<|start_header_id|>assistant<|end_header_id|>\n\n";
    return ss.str();
}

// --- 3. Mistral / Ministral ---
std::string MistralFormatter::format(const sentiric::llm::v1::GenerateStreamRequest& request, const Settings& settings) const {
    std::stringstream ss;
    std::string sys = !request.system_prompt().empty() ? request.system_prompt() : settings.template_system_prompt;
    
    // Mistral'de ilk kullanıcı mesajı sistem prompt'u ile birleşir.
    bool history_exists = request.history_size() > 0;

    ss << "[INST] ";
    if (!history_exists && !sys.empty()) {
        ss << sys << "\n\n";
    }

    for (const auto& turn : request.history()) {
        if (turn.role() == "user") {
            ss << turn.content() << " [/INST]";
        } else {
            ss << " " << turn.content() << "</s> [INST] ";
        }
    }

    std::string user_content = build_final_user_content(request, settings);

    ss << user_content << " [/INST]";
    return ss.str();
}

// --- 4. Gemma ---
std::string GemmaFormatter::format(const sentiric::llm::v1::GenerateStreamRequest& request, const Settings& settings) const {
    std::stringstream ss;
    std::string sys = !request.system_prompt().empty() ? request.system_prompt() : settings.template_system_prompt;
    
    // Gemma'da resmi system prompt desteği sınırlı. User prompt içine "Instruction" olarak gömüyoruz.
    // [FIX] Daha agresif instruction formatı.
    
    for (const auto& turn : request.history()) {
        std::string role = (turn.role() == "user") ? "user" : "model";
        ss << "<start_of_turn>" << role << "\n";
        ss << turn.content() << "<end_of_turn>\n";
    }

    ss << "<start_of_turn>user\n";
    if (!sys.empty()) {
        // Modelin bunu bir talimat olarak algılaması için başlık ve format güçlendirildi.
        ss << "IMPORTANT SYSTEM INSTRUCTION:\n" << sys << "\n\nUser Query:\n";
    }

    std::string user_content = build_final_user_content(request, settings);
    
    ss << user_content << "<end_of_turn>\n<start_of_turn>model\n";
    return ss.str();
}

// --- 5. Raw ---
std::string RawTemplateFormatter::format(const sentiric::llm::v1::GenerateStreamRequest& request, const Settings& settings) const {
    std::stringstream ss;
    std::string sys = !request.system_prompt().empty() ? request.system_prompt() : settings.template_system_prompt;
    
    ss << "System: " << sys << "\n\n";
    for (const auto& turn : request.history()) {
        ss << turn.role() << ": " << turn.content() << "\n";
    }
    
    std::string user_content = build_final_user_content(request, settings);
    
    ss << "User: " << user_content << "\nAssistant:";
    return ss.str();
}
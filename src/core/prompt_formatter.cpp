#include "core/prompt_formatter.h"
#include <sstream>
#include "spdlog/spdlog.h"
#include <algorithm>

// ==========================================
// 1. GEMMA FORMATTER
// ==========================================
std::string GemmaFormatter::format(const sentiric::llm::v1::LLMLocalServiceGenerateStreamRequest& request) const {
    std::ostringstream oss;
    
    std::string combined_system;
    if (!request.system_prompt().empty()) {
        combined_system += "Role & Instructions:\n" + request.system_prompt() + "\n\n";
    }
    if (request.has_rag_context() && !request.rag_context().empty()) {
        combined_system += "Context Data:\n" + request.rag_context() + "\n\n";
    }

    bool is_first_turn = true;
    for (const auto& turn : request.history()) {
        if (turn.role() == "user") {
            oss << "<start_of_turn>user\n";
            if (is_first_turn && !combined_system.empty()) {
                oss << combined_system;
                combined_system.clear(); 
                is_first_turn = false;
            }
            oss << turn.content() << "<end_of_turn>\n";
        } 
        else if (turn.role() == "assistant" || turn.role() == "model") {
            oss << "<start_of_turn>model\n" << turn.content() << "<end_of_turn>\n";
            is_first_turn = false;
        }
    }

    oss << "<start_of_turn>user\n";
    if (!combined_system.empty()) {
        oss << combined_system;
    }
    oss << request.user_prompt() << "<end_of_turn>\n";
    oss << "<start_of_turn>model\n";
    
    return oss.str();
}

std::vector<std::string> GemmaFormatter::get_stop_sequences() const {
    return { "<end_of_turn>", "<eos>", "user:", "model:" };
}

// ==========================================
// 2. LLAMA 3 FORMATTER
// ==========================================
std::string Llama3Formatter::format(const sentiric::llm::v1::LLMLocalServiceGenerateStreamRequest& request) const {
    std::ostringstream oss;
    oss << "<|begin_of_text|>";

    std::string system_content = request.system_prompt();
    if (request.has_rag_context() && !request.rag_context().empty()) {
        if(!system_content.empty()) system_content += "\n\n";
        system_content += "Context:\n" + request.rag_context();
    }

    if (!system_content.empty()) {
        oss << "<|start_header_id|>system<|end_header_id|>\n\n" << system_content << "<|eot_id|>";
    }

    for (const auto& turn : request.history()) {
        std::string role = (turn.role() == "user") ? "user" : "assistant";
        oss << "<|start_header_id|>" << role << "<|end_header_id|>\n\n" << turn.content() << "<|eot_id|>";
    }

    oss << "<|start_header_id|>user<|end_header_id|>\n\n" << request.user_prompt() << "<|eot_id|>";
    oss << "<|start_header_id|>assistant<|end_header_id|>\n\n";

    return oss.str();
}

std::vector<std::string> Llama3Formatter::get_stop_sequences() const {
    return { "<|eot_id|>", "<|end_of_text|>", "assistant:", "user:" };
}

// ==========================================
// 3. QWEN 2.5 FORMATTER (ChatML) - YENİ
// ==========================================
std::string QwenFormatter::format(const sentiric::llm::v1::LLMLocalServiceGenerateStreamRequest& request) const {
    std::ostringstream oss;
    
    // Qwen ChatML Format:
    // <|im_start|>system\n...<|im_end|>\n
    // <|im_start|>user\n...<|im_end|>\n
    // <|im_start|>assistant\n...<|im_end|>\n

    // A. System & RAG
    std::string system_content = request.system_prompt();
    if (request.has_rag_context() && !request.rag_context().empty()) {
        if(!system_content.empty()) system_content += "\n\n";
        system_content += "REFERANS BİLGİ (RAG):\n" + request.rag_context();
    }

    if (!system_content.empty()) {
        oss << "<|im_start|>system\n" << system_content << "<|im_end|>\n";
    }

    // B. History
    for (const auto& turn : request.history()) {
        std::string role = (turn.role() == "user") ? "user" : "assistant";
        oss << "<|im_start|>" << role << "\n" << turn.content() << "<|im_end|>\n";
    }

    // C. Current User
    oss << "<|im_start|>user\n" << request.user_prompt() << "<|im_end|>\n";

    // D. Assistant Trigger
    oss << "<|im_start|>assistant\n";

    return oss.str();
}

std::vector<std::string> QwenFormatter::get_stop_sequences() const {
    return { 
        "<|im_end|>", 
        "<|endoftext|>", 
        "<|im_start|>" // Halüsinasyonla yeni turn başlatırsa kes
    };
}

// ==========================================
// 4. FACTORY
// ==========================================
std::unique_ptr<PromptFormatter> create_formatter_for_model(const std::string& model_architecture) {
    std::string arch = model_architecture;
    std::transform(arch.begin(), arch.end(), arch.begin(), ::tolower);

    spdlog::info("Creating formatter for architecture: {}", arch);

    if (arch.find("llama") != std::string::npos) {
        return std::make_unique<Llama3Formatter>();
    } 
    if (arch.find("qwen") != std::string::npos) {
        return std::make_unique<QwenFormatter>();
    }
    
    // Varsayılan (Gemma)
    return std::make_unique<GemmaFormatter>();
}
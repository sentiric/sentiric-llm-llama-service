#include "core/prompt_formatter.h"
#include "spdlog/spdlog.h"
#include <vector>
#include <cstring>
#include <sstream>
#include <iostream>
#include <list> 

std::unique_ptr<PromptFormatter> create_formatter() {
    spdlog::info("Creating NativeTemplateFormatter (strict GGUF adherence)");
    return std::make_unique<NativeTemplateFormatter>();
}

std::string NativeTemplateFormatter::format(const sentiric::llm::v1::GenerateStreamRequest& request, const llama_model* model) const {
    if (!model) return "";

    std::vector<char> tmpl_buf(4096); 
    int32_t tmpl_len = llama_model_meta_val_str(model, "tokenizer.chat_template", tmpl_buf.data(), tmpl_buf.size());

    std::string template_to_use;
    if (tmpl_len > 0) {
        template_to_use = std::string(tmpl_buf.data());
    } else {
        spdlog::warn("⚠️ Model metadata missing 'tokenizer.chat_template'. Falling back to generic ChatML.");
        template_to_use = "{% for message in messages %}{{'<|im_start|>' + message['role'] + '\n' + message['content'] + '<|im_end|>' + '\n'}}{% endfor %}{% if add_generation_prompt %}{{ '<|im_start|>assistant\n' }}{% endif %}";
    }

    std::list<std::string> content_storage; 
    std::vector<llama_chat_message> messages;
    
    // --- System Prompt & RAG Logic ---
    std::string base_system = request.system_prompt();
    std::string final_system_content;

    // RAG varsa daha katı ve yapılandırılmış bir prompt kullan
    if (request.has_rag_context() && !request.rag_context().empty()) {
        std::stringstream ss;
        if (!base_system.empty()) {
            ss << base_system << "\n\n";
        }
        
        // Bu yapı, Qwen ve Llama 3 için RAG performansını artırır
        ss << "### CONTEXT / BILGI NOTU:\n" 
           << request.rag_context() 
           << "\n\n### YONERGE:\n"
           << "Kullanicinin sorusunu SADECE yukaridaki Context bilgilerini kullanarak cevapla. "
           << "Bilgi context icinde yoksa 'Bilmiyorum' de. Asla uydurma.";
           
        final_system_content = ss.str();
    } else {
        // RAG yoksa, sadece varsa system prompt'u kullan
        final_system_content = base_system;
    }

    if (!final_system_content.empty()) {
        content_storage.push_back(final_system_content);
        messages.push_back({"system", content_storage.back().c_str()});
    }

    // --- History ---
    for (const auto& turn : request.history()) {
        std::string role = (turn.role() == "user") ? "user" : "assistant";
        content_storage.push_back(turn.content());
        messages.push_back({role.c_str(), content_storage.back().c_str()});
    }

    // --- Current User Prompt ---
    if (!request.user_prompt().empty()) {
        content_storage.push_back(request.user_prompt());
        messages.push_back({"user", content_storage.back().c_str()});
    }

    // Apply Template
    int32_t required_size = llama_chat_apply_template(
        template_to_use.c_str(), 
        messages.data(), 
        messages.size(), 
        true, 
        nullptr, 
        0
    );

    if (required_size < 0) {
        // Hata durumunda fallback
        std::ostringstream oss;
        if(!final_system_content.empty()) oss << final_system_content << "\n";
        oss << request.user_prompt();
        return oss.str();
    }

    std::string formatted_output;
    formatted_output.resize(required_size);
    
    llama_chat_apply_template(
        template_to_use.c_str(), 
        messages.data(), 
        messages.size(), 
        true, 
        &formatted_output[0], 
        formatted_output.size()
    );

    return formatted_output;
}
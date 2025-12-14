#include "core/prompt_formatter.h"
#include "spdlog/spdlog.h"
#include <vector>
#include <cstring>
#include <sstream>
#include <iostream>

std::unique_ptr<PromptFormatter> create_formatter() {
    spdlog::info("Creating NativeTemplateFormatter (using GGUF embedded chat template)");
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
        // Fallback: Standart ChatML (Güvenli, yaygın)
        spdlog::warn("⚠️ Model does not have a native chat template. Using ChatML fallback.");
        template_to_use = "{% for message in messages %}{{'<|im_start|>' + message['role'] + '\n' + message['content'] + '<|im_end|>' + '\n'}}{% endfor %}{% if add_generation_prompt %}{{ '<|im_start|>assistant\n' }}{% endif %}";
    }

    std::vector<llama_chat_message> messages;
    std::vector<std::string> content_storage;
    
    // System Prompt Logic (Request or Fallback handled in Controller, but here we enforce structure)
    std::string effective_system_prompt = request.system_prompt();
    if (request.has_rag_context() && !request.rag_context().empty()) {
        if (!effective_system_prompt.empty()) effective_system_prompt += "\n\n";
        effective_system_prompt += "CONTEXT:\n" + request.rag_context();
    }

    if (!effective_system_prompt.empty()) {
        content_storage.push_back(effective_system_prompt);
        messages.push_back({"system", content_storage.back().c_str()});
    }

    for (const auto& turn : request.history()) {
        std::string role = (turn.role() == "user") ? "user" : "assistant";
        content_storage.push_back(turn.content());
        messages.push_back({role.c_str(), content_storage.back().c_str()});
    }

    if (!request.user_prompt().empty()) {
        content_storage.push_back(request.user_prompt());
        messages.push_back({"user", content_storage.back().c_str()});
    }

    // --- Template Application ---
    // add_generation_prompt=true ensures the model knows it's time to speak.
    int32_t required_size = llama_chat_apply_template(
        template_to_use.c_str(), 
        messages.data(), 
        messages.size(), 
        true, 
        nullptr, 
        0
    );

    if (required_size < 0) {
        spdlog::error("❌ llama_chat_apply_template failed. Fallback to manual concat.");
        // Manuel fallback (Very reliable last resort)
        std::ostringstream oss;
        if(!effective_system_prompt.empty()) 
            oss << "System: " << effective_system_prompt << "\n\n";
        
        for(const auto& turn : request.history()) {
            std::string r = (turn.role() == "user") ? "User" : "Assistant";
            oss << r << ": " << turn.content() << "\n\n";
        }
        oss << "User: " << request.user_prompt() << "\n\nAssistant: ";
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
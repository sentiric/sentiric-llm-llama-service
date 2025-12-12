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
        // Fallback: Standart ChatML (Qwen, vb. için en güvenlisi)
        template_to_use = "{% for message in messages %}{{'<|im_start|>' + message['role'] + '\n' + message['content'] + '<|im_end|>' + '\n'}}{% endfor %}{% if add_generation_prompt %}{{ '<|im_start|>assistant\n' }}{% endif %}";
    }

    std::vector<llama_chat_message> messages;
    std::vector<std::string> content_storage;
    
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

    // --- KRİTİK: add_generation_prompt ---
    // Bu parametre 'true' olduğunda, llama.cpp şablonun sonuna
    // "<|im_start|>assistant\n" (veya modelin eşdeğerini) ekler.
    // Bu, modelin "user said..." diye sohbete başlamasını engeller,
    // direkt cevap vermeye zorlar.
    
    int32_t required_size = llama_chat_apply_template(
        template_to_use.c_str(), 
        messages.data(), 
        messages.size(), 
        true, // add_generation_prompt = TRUE (ZORUNLU)
        nullptr, 
        0
    );

    if (required_size < 0) {
        // Eğer native template başarısız olursa, manuel ChatML uygula
        std::ostringstream oss;
        if(!effective_system_prompt.empty()) 
            oss << "<|im_start|>system\n" << effective_system_prompt << "<|im_end|>\n";
        
        for(const auto& turn : request.history()) {
            std::string r = (turn.role() == "user") ? "user" : "assistant";
            oss << "<|im_start|>" << r << "\n" << turn.content() << "<|im_end|>\n";
        }
        oss << "<|im_start|>user\n" << request.user_prompt() << "<|im_end|>\n";
        oss << "<|im_start|>assistant\n"; // Prompt Injection (CoT Engelleme)
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
#include "core/prompt_formatter.h"
#include "spdlog/spdlog.h"
#include <vector>
#include <cstring>
#include <list> 

std::unique_ptr<PromptFormatter> create_formatter() {
    spdlog::info("Creating NativeTemplateFormatter (strict GGUF adherence)");
    return std::make_unique<NativeTemplateFormatter>();
}

// Helper function for simple string replacement
void replace_all(std::string& str, const std::string& from, const std::string& to) {
    if(from.empty()) return;
    size_t start_pos = 0;
    while((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
}

std::string NativeTemplateFormatter::format(const sentiric::llm::v1::GenerateStreamRequest& request, const llama_model* model, const Settings& settings) const {
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
    
    // --- System Prompt & RAG Logic (Template-driven) ---
    // [FIX] request.has_system_prompt() yerine !empty() kontrolü
    std::string base_system_prompt = !request.system_prompt().empty()
                                     ? request.system_prompt() 
                                     : settings.template_system_prompt;

    // 2. RAG varsa, RAG şablonunu uygula
    if (request.has_rag_context() && !request.rag_context().empty()) {
        std::string rag_template = settings.template_rag_prompt;
        replace_all(rag_template, "{{rag_context}}", request.rag_context());
        // RAG şablonu, base_system_prompt'u içerebilir veya içermeyebilir.
        replace_all(rag_template, "{{system_prompt}}", base_system_prompt);
        
        content_storage.push_back(rag_template);
    } else {
        content_storage.push_back(base_system_prompt);
    }

    // Nihai system prompt'u ekle (boş değilse)
    if (!content_storage.back().empty()) {
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
        spdlog::error("Failed to apply chat template. required_size={}", required_size);
        return request.user_prompt(); // Fallback to raw prompt
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
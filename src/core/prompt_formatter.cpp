#include "core/prompt_formatter.h"
#include "spdlog/spdlog.h"
#include <vector>
#include <cstring>
#include <sstream>
#include <iostream>
#include <list> // Vector yerine List kullanımı (Pointer Stability)

std::unique_ptr<PromptFormatter> create_formatter() {
    spdlog::info("Creating NativeTemplateFormatter (strict GGUF adherence)");
    return std::make_unique<NativeTemplateFormatter>();
}

std::string NativeTemplateFormatter::format(const sentiric::llm::v1::GenerateStreamRequest& request, const llama_model* model) const {
    if (!model) return "";

    // 1. Modelin kendi şablonunu al
    std::vector<char> tmpl_buf(4096); 
    int32_t tmpl_len = llama_model_meta_val_str(model, "tokenizer.chat_template", tmpl_buf.data(), tmpl_buf.size());

    std::string template_to_use;
    if (tmpl_len > 0) {
        template_to_use = std::string(tmpl_buf.data());
    } else {
        // Modelde şablon yoksa, temel bir ChatML varsayımı yapabiliriz ama loglayalım.
        spdlog::warn("⚠️ Model metadata missing 'tokenizer.chat_template'. Falling back to generic ChatML.");
        template_to_use = "{% for message in messages %}{{'<|im_start|>' + message['role'] + '\n' + message['content'] + '<|im_end|>' + '\n'}}{% endfor %}{% if add_generation_prompt %}{{ '<|im_start|>assistant\n' }}{% endif %}";
    }

    // 2. Mesajları hazırla
    // KRİTİK DÜZELTME: std::vector<std::string> kullanıldığında, push_back yapıldıkça
    // vector resize olur ve önceki stringlerin bellek adresleri değişir (Invalid Pointer).
    // std::list kullanarak stringlerin bellekte sabit kalmasını garanti ediyoruz.
    std::list<std::string> content_storage; 
    std::vector<llama_chat_message> messages;
    
    // --- System Prompt & RAG ---
    std::string effective_system_prompt = request.system_prompt();
    if (request.has_rag_context() && !request.rag_context().empty()) {
        if (!effective_system_prompt.empty()) effective_system_prompt += "\n\n";
        effective_system_prompt += "CONTEXT DATA:\n" + request.rag_context();
    }

    if (!effective_system_prompt.empty()) {
        content_storage.push_back(effective_system_prompt);
        // list::back() referansı asla geçersiz kalmaz (list eleman silinmediği sürece)
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

    // 3. Template Uygula (Native API)
    // add_generation_prompt=true: Modelin cevaba başlaması için gerekli tokenları ekler.
    int32_t required_size = llama_chat_apply_template(
        template_to_use.c_str(), 
        messages.data(), 
        messages.size(), 
        true, 
        nullptr, 
        0
    );

    if (required_size < 0) {
        spdlog::error("❌ llama_chat_apply_template failed. Model template might be broken.");
        // Fallback: Raw concatenation (En kötü senaryo)
        std::ostringstream oss;
        if(!effective_system_prompt.empty()) oss << effective_system_prompt << "\n";
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
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
    if (!model) {
        spdlog::error("NativeTemplateFormatter: Model is null!");
        return "";
    }

    // --- ADIM 1: Modelden Şablonu Çek (Fixing API Mismatch) ---
    // Model metadata'sından "tokenizer.chat_template" değerini okumaya çalışıyoruz.
    // Bu değer genellikle 2048 byte'tan küçüktür, güvenli bir buffer açıyoruz.
    std::vector<char> tmpl_buf(4096); 
    int32_t tmpl_len = llama_model_meta_val_str(model, "tokenizer.chat_template", tmpl_buf.data(), tmpl_buf.size());

    std::string template_to_use;

    if (tmpl_len > 0) {
        // Modelin içinde gömülü şablon bulundu
        template_to_use = std::string(tmpl_buf.data());
        spdlog::trace("Using embedded GGUF chat template.");
    } else {
        // Modelde şablon yoksa veya okunamadıysa FALLBACK kullan
        // Çoğu modern model (Qwen, Llama 3, Phi-3, Mistral) ChatML veya benzeri yapıları destekler.
        // Buraya güvenli bir generic şablon koyuyoruz.
        spdlog::warn("⚠️ 'tokenizer.chat_template' not found in model metadata. Using generic ChatML fallback.");
        template_to_use = "{% for message in messages %}{{'<|im_start|>' + message['role'] + '\n' + message['content'] + '<|im_end|>' + '\n'}}{% endfor %}{% if add_generation_prompt %}{{ '<|im_start|>assistant\n' }}{% endif %}";
    }

    // --- ADIM 2: Mesajları Hazırla ---
    std::vector<llama_chat_message> messages;
    
    // Pointer saklama için kalıcı buffer
    std::vector<std::string> content_storage;
    
    // RAG ve System Prompt birleşimi
    std::string effective_system_prompt = request.system_prompt();
    if (request.has_rag_context() && !request.rag_context().empty()) {
        if (!effective_system_prompt.empty()) effective_system_prompt += "\n\n";
        effective_system_prompt += "CONTEXT DATA:\n" + request.rag_context();
    }

    // System mesajını ekle
    if (!effective_system_prompt.empty()) {
        content_storage.push_back(effective_system_prompt);
        messages.push_back({"system", content_storage.back().c_str()});
    }

    // Geçmişi ekle
    for (const auto& turn : request.history()) {
        std::string role = (turn.role() == "user") ? "user" : "assistant";
        content_storage.push_back(turn.content());
        messages.push_back({role.c_str(), content_storage.back().c_str()});
    }

    // Mevcut kullanıcı mesajını ekle
    if (!request.user_prompt().empty()) {
        content_storage.push_back(request.user_prompt());
        messages.push_back({"user", content_storage.back().c_str()});
    }

    // --- ADIM 3: Şablonu Uygula (Boyut Hesaplama) ---
    // ARTIK MODELİ DEĞİL, STRING OLAN 'template_to_use' DEĞİŞKENİNİ VERİYORUZ.
    int32_t required_size = llama_chat_apply_template(
        template_to_use.c_str(), 
        messages.data(), 
        messages.size(), 
        true, // add_generation_prompt (asistanın cevabını başlat)
        nullptr, 
        0
    );

    if (required_size < 0) {
        spdlog::error("llama_chat_apply_template failed to calculate size.");
        // Çok kötü bir durum olursa basit string birleştirme
        std::ostringstream fallback;
        for(const auto& msg : messages) {
            fallback << msg.role << ": " << msg.content << "\n";
        }
        fallback << "assistant: ";
        return fallback.str();
    }

    // --- ADIM 4: Gerçek Formatlama ---
    std::string formatted_output;
    formatted_output.resize(required_size);
    
    int32_t final_size = llama_chat_apply_template(
        template_to_use.c_str(), 
        messages.data(), 
        messages.size(), 
        true, 
        &formatted_output[0], 
        formatted_output.size()
    );

    if (final_size < 0) {
         spdlog::error("llama_chat_apply_template failed during formatting.");
         return "";
    }

    return formatted_output;
}
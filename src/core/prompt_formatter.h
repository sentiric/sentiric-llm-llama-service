#pragma once
#include "sentiric/llm/v1/llama.pb.h"
#include "llama.h"
#include "config.h"
#include <string>
#include <memory>
#include <algorithm>

class PromptFormatter {
public:
    virtual ~PromptFormatter() = default;
    virtual std::string format(const sentiric::llm::v1::GenerateStreamRequest& request, const llama_model* model, const Settings& settings) const = 0;
    
    // Yardımcı: String içindeki tokenları değiştirir
    static void replace_all(std::string& str, const std::string& from, const std::string& to) {
        if(from.empty()) return;
        size_t start_pos = 0;
        while((start_pos = str.find(from, start_pos)) != std::string::npos) {
            str.replace(start_pos, from.length(), to);
            start_pos += to.length();
        }
    }
};

// 1. Qwen (ChatML) Formatter
// Örnek: <|im_start|>user\nMsg<|im_end|>
class QwenChatMLFormatter : public PromptFormatter {
public:
    std::string format(const sentiric::llm::v1::GenerateStreamRequest& request, const llama_model* model, const Settings& settings) const override;
};

// 2. Llama 3 Formatter
// Örnek: <|start_header_id|>user<|end_header_id|>\n\nMsg<|eot_id|>
class Llama3Formatter : public PromptFormatter {
public:
    std::string format(const sentiric::llm::v1::GenerateStreamRequest& request, const llama_model* model, const Settings& settings) const override;
};

// 3. Gemma Formatter
// Örnek: <start_of_turn>user\nMsg<end_of_turn>
class GemmaFormatter : public PromptFormatter {
public:
    std::string format(const sentiric::llm::v1::GenerateStreamRequest& request, const llama_model* model, const Settings& settings) const override;
};

// 4. Raw/Template Formatter (Generic Fallback)
// profiles.json içindeki şablonları doğrudan kullanır.
class RawTemplateFormatter : public PromptFormatter {
public:
    std::string format(const sentiric::llm::v1::GenerateStreamRequest& request, const llama_model* model, const Settings& settings) const override;
};

// Factory Function
std::unique_ptr<PromptFormatter> create_formatter(const std::string& model_id);
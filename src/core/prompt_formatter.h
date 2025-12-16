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
    
    static void replace_all(std::string& str, const std::string& from, const std::string& to) {
        if(from.empty()) return;
        size_t start_pos = 0;
        while((start_pos = str.find(from, start_pos)) != std::string::npos) {
            str.replace(start_pos, from.length(), to);
            start_pos += to.length();
        }
    }
};

class QwenChatMLFormatter : public PromptFormatter {
public:
    std::string format(const sentiric::llm::v1::GenerateStreamRequest& request, const llama_model* model, const Settings& settings) const override;
};

class Llama3Formatter : public PromptFormatter {
public:
    std::string format(const sentiric::llm::v1::GenerateStreamRequest& request, const llama_model* model, const Settings& settings) const override;
};

// YENİ: Mistral Formatter
class MistralFormatter : public PromptFormatter {
public:
    std::string format(const sentiric::llm::v1::GenerateStreamRequest& request, const llama_model* model, const Settings& settings) const override;
};

class GemmaFormatter : public PromptFormatter {
public:
    std::string format(const sentiric::llm::v1::GenerateStreamRequest& request, const llama_model* model, const Settings& settings) const override;
};

class RawTemplateFormatter : public PromptFormatter {
public:
    std::string format(const sentiric::llm::v1::GenerateStreamRequest& request, const llama_model* model, const Settings& settings) const override;
};

std::unique_ptr<PromptFormatter> create_formatter(const std::string& model_id);
#pragma once

#include "sentiric/llm/v1/local.pb.h"
#include <string>
#include <vector>
#include <memory>

// Arayüz (Interface)
class PromptFormatter {
public:
    virtual ~PromptFormatter() = default;
    virtual std::string format(const sentiric::llm::v1::LLMLocalServiceGenerateStreamRequest& request) const = 0;
    virtual std::vector<std::string> get_stop_sequences() const = 0;
};

// Gemma (Google)
class GemmaFormatter : public PromptFormatter {
public:
    std::string format(const sentiric::llm::v1::LLMLocalServiceGenerateStreamRequest& request) const override;
    std::vector<std::string> get_stop_sequences() const override;
};

// Llama 3 (Meta)
class Llama3Formatter : public PromptFormatter {
public:
    std::string format(const sentiric::llm::v1::LLMLocalServiceGenerateStreamRequest& request) const override;
    std::vector<std::string> get_stop_sequences() const override;
};

// Qwen 2.5 (Alibaba) - YENİ EKLENDİ
class QwenFormatter : public PromptFormatter {
public:
    std::string format(const sentiric::llm::v1::LLMLocalServiceGenerateStreamRequest& request) const override;
    std::vector<std::string> get_stop_sequences() const override;
};

// Fabrika
std::unique_ptr<PromptFormatter> create_formatter_for_model(const std::string& model_architecture);
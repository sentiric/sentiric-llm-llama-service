#pragma once

#include "sentiric/llm/v1/llama.pb.h"
#include "llama.h"
#include "config.h"
#include <string>
#include <memory>

class PromptFormatter {
public:
    virtual ~PromptFormatter() = default;
    virtual std::string format(const sentiric::llm::v1::GenerateStreamRequest& request, const llama_model* model, const Settings& settings) const = 0;
};

// Metadata'ya güvenmeyen, Qwen/Llama uyumlu ChatML formatter
class ExplicitChatMLFormatter : public PromptFormatter {
public:
    std::string format(const sentiric::llm::v1::GenerateStreamRequest& request, const llama_model* model, const Settings& settings) const override;
};

std::unique_ptr<PromptFormatter> create_formatter();
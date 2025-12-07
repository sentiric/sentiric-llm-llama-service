#pragma once

#include "sentiric/llm/v1/llama.pb.h"
#include <string>
#include <vector>
#include <memory>

class PromptFormatter {
public:
    virtual ~PromptFormatter() = default;
    // DÜZELTME: GenerateStreamRequest
    virtual std::string format(const sentiric::llm::v1::GenerateStreamRequest& request) const = 0;
    virtual std::vector<std::string> get_stop_sequences() const = 0;
};

class GemmaFormatter : public PromptFormatter {
public:
    std::string format(const sentiric::llm::v1::GenerateStreamRequest& request) const override;
    std::vector<std::string> get_stop_sequences() const override;
};

class Llama3Formatter : public PromptFormatter {
public:
    std::string format(const sentiric::llm::v1::GenerateStreamRequest& request) const override;
    std::vector<std::string> get_stop_sequences() const override;
};

class QwenFormatter : public PromptFormatter {
public:
    std::string format(const sentiric::llm::v1::GenerateStreamRequest& request) const override;
    std::vector<std::string> get_stop_sequences() const override;
};

std::unique_ptr<PromptFormatter> create_formatter_for_model(const std::string& model_architecture);
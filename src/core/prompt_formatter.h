// src/core/prompt_formatter.h
#pragma once

#include "sentiric/llm/v1/local.pb.h"
#include <string>
#include <vector>
#include <memory>

// Arayüz (Soyut Temel Sınıf)
class PromptFormatter {
public:
    virtual ~PromptFormatter() = default;
    virtual std::string format(const sentiric::llm::v1::LLMLocalServiceGenerateStreamRequest& request) const = 0;
    virtual std::vector<std::string> get_stop_sequences() const = 0;
};

// Gemma 3 için somut implementasyon
class Gemma3Formatter : public PromptFormatter {
public:
    std::string format(const sentiric::llm::v1::LLMLocalServiceGenerateStreamRequest& request) const override;
    std::vector<std::string> get_stop_sequences() const override;
};

// Fabrika Fonksiyonu: Model mimarisine göre doğru formatlayıcıyı oluşturur.
std::unique_ptr<PromptFormatter> create_formatter_for_model(const std::string& model_architecture);
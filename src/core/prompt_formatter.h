#pragma once

#include "sentiric/llm/v1/llama.pb.h"
#include "llama.h"
#include "config.h" // Settings struct'ı için eklendi
#include <string>
#include <vector>
#include <memory>

class PromptFormatter {
public:
    virtual ~PromptFormatter() = default;
    
    // İmza değiştirildi: Artık Settings nesnesini de alıyor.
    virtual std::string format(const sentiric::llm::v1::GenerateStreamRequest& request, const llama_model* model, const Settings& settings) const = 0;
};

class NativeTemplateFormatter : public PromptFormatter {
public:
    std::string format(const sentiric::llm::v1::GenerateStreamRequest& request, const llama_model* model, const Settings& settings) const override;
};

std::unique_ptr<PromptFormatter> create_formatter();
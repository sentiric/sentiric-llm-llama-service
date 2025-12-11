#pragma once

#include "sentiric/llm/v1/llama.pb.h"
#include "llama.h"
#include <string>
#include <vector>
#include <memory>

class PromptFormatter {
public:
    virtual ~PromptFormatter() = default;
    
    // GGUF içerisindeki template'i kullanarak formatlama yapar.
    // Model pointer'ı gerektirir çünkü template modelin içindedir.
    virtual std::string format(const sentiric::llm::v1::GenerateStreamRequest& request, const llama_model* model) const = 0;
};

// llama.cpp'nin 'llama_chat_apply_template' fonksiyonunu kullanan evrensel formatter.
class NativeTemplateFormatter : public PromptFormatter {
public:
    std::string format(const sentiric::llm::v1::GenerateStreamRequest& request, const llama_model* model) const override;
};

// Factory fonksiyonu artık model mimarisine bakmaksızın tek tip döndürür.
std::unique_ptr<PromptFormatter> create_formatter();
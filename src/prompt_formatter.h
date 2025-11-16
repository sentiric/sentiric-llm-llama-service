#pragma once

#include "sentiric/llm/v1/local.pb.h"
#include <string>

class PromptFormatter {
public:
    static std::string format(const sentiric::llm::v1::LLMLocalServiceGenerateStreamRequest& request);
};
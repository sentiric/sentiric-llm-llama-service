#pragma once
#include "sentiric/llm/v1/local.pb.h"
#include <string>
#include <vector>

class PromptFormatter {
public:
    static std::string format(const sentiric::llm::v1::LLMLocalServiceGenerateStreamRequest& request);
    static std::vector<std::string> get_stop_sequences();
};
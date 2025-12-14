#pragma once

#include "llm_engine.h"
#include "httplib.h"
#include "nlohmann/json.hpp"
#include <memory>
#include <string>

class ChatController {
public:
    explicit ChatController(std::shared_ptr<LLMEngine> engine);

    // Ana handler fonksiyonu
    void handle_chat_completions(const httplib::Request& req, httplib::Response& res);

private:
    std::shared_ptr<LLMEngine> engine_;

    // Yardımcı fonksiyonlar (Private implementation details)
    bool has_incomplete_utf8_suffix(const std::string& str);
    std::string sanitize_token(const std::string& input);
    std::string get_reasoning_instruction(const std::string& level);
    
    // İstek işleme adımları
    sentiric::llm::v1::GenerateStreamRequest build_grpc_request(const nlohmann::json& body, const std::string& reasoning_prompt);
    void handle_streaming_response(std::shared_ptr<BatchedRequest> batched_request, httplib::Response& res);
    void handle_unary_response(std::shared_ptr<BatchedRequest> batched_request, std::future<void>& completion_future, httplib::Response& res);
};
#pragma once

#include <memory>
#include <string>
#include <functional>
#include <grpcpp/grpcpp.h>
#include "sentiric/llm/v1/llama.grpc.pb.h"

namespace sentiric_llm_cli {

class GRPCClient {
public:
    GRPCClient(const std::string& endpoint);
    ~GRPCClient();
    
    bool is_connected();
    
    // DÜZELTME: GenerateStreamRequest (Eski: LlamaGenerateStreamRequest)
    bool generate_stream(
        const sentiric::llm::v1::GenerateStreamRequest& request,
        std::function<void(const std::string&)> on_token
    );
    
    void set_timeout(int seconds);

private:
    void ensure_channel_is_ready();
    std::string endpoint_;
    int timeout_seconds_ = 120;
    // DÜZELTME: LlamaService (Eski: LLMLocalService)
    std::unique_ptr<sentiric::llm::v1::LlamaService::Stub> stub_;
    std::shared_ptr<grpc::Channel> channel_;
};

} // namespace sentiric_llm_cli
#pragma once

#include <memory>
#include <string>
#include <functional>
#include <mutex>
#include <grpcpp/grpcpp.h>
#include "sentiric/llm/v1/llama.grpc.pb.h"

namespace sentiric_llm_cli {

class GRPCClient {
public:
    GRPCClient(const std::string& endpoint);
    ~GRPCClient();
    
    bool is_connected();
    
    bool generate_stream(
        const sentiric::llm::v1::GenerateStreamRequest& request,
        std::function<void(const std::string&)> on_token
    );
    
    bool generate_stream_with_cancellation(
        const sentiric::llm::v1::GenerateStreamRequest& request,
        std::function<void(const std::string&)> on_token
    );
    void cancel_stream();

    void set_timeout(int seconds);

private:
    void ensure_channel_is_ready();
    std::string endpoint_;
    int timeout_seconds_ = 120;
    std::unique_ptr<sentiric::llm::v1::LlamaService::Stub> stub_;
    std::shared_ptr<grpc::Channel> channel_;
    
    std::shared_ptr<grpc::ClientContext> cancellable_context_;
    std::mutex context_mutex_;
};

} // namespace sentiric_llm_cli
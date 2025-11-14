#pragma once

#include <memory>
#include <string>
#include <functional>
#include <grpcpp/grpcpp.h>
#include "sentiric/llm/v1/local.grpc.pb.h"

namespace sentiric_llm_cli {

class GRPCClient {
public:
    GRPCClient(const std::string& endpoint);
    ~GRPCClient();
    
    // DÜZELTİLDİ: const kaldırıldı.
    bool is_connected();
    
    bool generate_stream(const std::string& prompt,
                        std::function<void(const std::string&)> on_token,
                        float temperature = 0.8f,
                        int max_tokens = 2048);
    
    void set_timeout(int seconds);

private:
    void ensure_channel_is_ready(); // YENİ yardımcı metod
    std::string endpoint_;
    int timeout_seconds_ = 30;
    std::unique_ptr<sentiric::llm::v1::LLMLocalService::Stub> stub_;
    std::shared_ptr<grpc::Channel> channel_;
};

} // namespace sentiric_llm_cli
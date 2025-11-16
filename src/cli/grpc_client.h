#pragma once

#include <memory>
#include <string>
#include <functional>
#include <grpcpp/grpcpp.h>
#include "sentiric/llm/v1/local.grpc.pb.h" // Kendi servis tanımımızı dahil ediyoruz

namespace sentiric_llm_cli {

class GRPCClient {
public:
    GRPCClient(const std::string& endpoint);
    ~GRPCClient();
    
    bool is_connected();
    
    // GÜNCELLENDİ: Artık basit string yerine tam request nesnesini kabul ediyor.
    bool generate_stream(
        const sentiric::llm::v1::LLMLocalServiceGenerateStreamRequest& request,
        std::function<void(const std::string&)> on_token
    );
    
    void set_timeout(int seconds);

private:
    void ensure_channel_is_ready();
    std::string endpoint_;
    int timeout_seconds_ = 120; // Varsayılan timeout artırıldı
    std::unique_ptr<sentiric::llm::v1::LLMLocalService::Stub> stub_;
    std::shared_ptr<grpc::Channel> channel_;
};

} // namespace sentiric_llm_cli
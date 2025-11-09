// src/grpc_test_client.cpp

#include <iostream>
#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include "sentiric/llm/v1/local.grpc.pb.h"
#include "spdlog/spdlog.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using sentiric::llm::v1::LLMLocalService;
// DÜZELTME 1: Her iki istek tipi de dahil ediliyor
using sentiric::llm::v1::LocalGenerateStreamRequest; 
using sentiric::llm::v1::LocalGenerateStreamResponse;
using sentiric::llm::v1::GenerationParams;

class LlamaClient {
public:
    LlamaClient(std::shared_ptr<Channel> channel)
        : stub_(LLMLocalService::NewStub(channel)) {}

    void GenerateStream(const std::string& prompt) {
        // DÜZELTME 2: Doğru istek tipi kullanılıyor (LocalGenerateStreamRequest)
        LocalGenerateStreamRequest request;
        request.set_prompt(prompt);

        // İsteğe bağlı parametreleri ayarla
        auto* params = request.mutable_params();
        params->set_temperature(0.7f);
        params->set_max_new_tokens(256);

        ClientContext context;
        auto reader = stub_->LocalGenerateStream(&context, request);
        LocalGenerateStreamResponse response;

        std::cout << "--- AI Response ---" << std::endl;
        while (reader->Read(&response)) {
            if (response.has_token()) {
                std::cout << response.token() << std::flush;
            } else if (response.has_finish_details()) {
                std::cout << "\n--- Stream Finished ---" << std::endl;
                spdlog::info("Finish Reason: {}", response.finish_details().finish_reason());
            }
        }
        std::cout << std::endl;

        Status status = reader->Finish();
        if (!status.ok()) {
            // DÜZELTME 3: status.error_code() bir tamsayıya cast ediliyor
            spdlog::error("RPC failed: {} ({})", status.error_message(), static_cast<int>(status.error_code()));
        } else {
            spdlog::info("Stream completed successfully.");
        }
    }

private:
    std::unique_ptr<LLMLocalService::Stub> stub_;
};

int main(int argc, char** argv) {
    spdlog::set_level(spdlog::level::info);
    std::string server_address = "localhost:16061";
    
    std::string prompt = "Türkiye'nin başkenti neresidir?";
    if (argc > 1) {
        prompt = argv[1];
    }
    
    LlamaClient client(grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials()));
    spdlog::info("Sending prompt: '{}'", prompt);
    client.GenerateStream(prompt);

    return 0;
}
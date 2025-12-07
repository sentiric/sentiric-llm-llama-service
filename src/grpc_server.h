#pragma once

#include "llm_engine.h"
#include "http_server.h" 
#include <memory>
#include <grpcpp/grpcpp.h>
#include "sentiric/llm/v1/llama.grpc.pb.h"

class GrpcServer final : public sentiric::llm::v1::LlamaService::Service {
public:
    explicit GrpcServer(std::shared_ptr<LLMEngine> engine, AppMetrics& metrics);

    grpc::Status GenerateStream(
        grpc::ServerContext* context,
        // DÜZELTME: GenerateStreamRequest
        const sentiric::llm::v1::GenerateStreamRequest* request,
        // DÜZELTME: GenerateStreamResponse
        grpc::ServerWriter<sentiric::llm::v1::GenerateStreamResponse>* writer
    ) override;

private:
    std::shared_ptr<LLMEngine> engine_;
    AppMetrics& metrics_;
};
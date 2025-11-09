#pragma once

#include "llm_engine.h"
#include <memory>
#include <grpcpp/grpcpp.h>
// DÜZELTME: Include yolu basitleştirildi.
#include "sentiric/llm/v1/local.grpc.pb.h"

class GrpcServer final : public sentiric::llm::v1::LLMLocalService::Service {
public:
    explicit GrpcServer(std::shared_ptr<LLMEngine> engine);

    grpc::Status LocalGenerateStream(
        grpc::ServerContext* context,
        const sentiric::llm::v1::LocalGenerateRequest* request,
        grpc::ServerWriter<sentiric::llm::v1::LocalGenerateStreamResponse>* writer
    ) override;

private:
    std::shared_ptr<LLMEngine> engine_;
};

void run_grpc_server(std::shared_ptr<LLMEngine> engine, int port);
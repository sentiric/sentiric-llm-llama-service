#pragma once

#include "llm_engine.h"
#include <memory>
#include <grpcpp/grpcpp.h>
#include "sentiric/llm/v1/local.grpc.pb.h"

class GrpcServer final : public sentiric::llm::v1::LLMLocalService::Service {
public:
    explicit GrpcServer(std::shared_ptr<LLMEngine> engine);

    // KONTROL EDİN: Bu metodun adının "GenerateStream" olduğundan emin olun.
    grpc::Status GenerateStream(
        grpc::ServerContext* context,
        const sentiric::llm::v1::LLMLocalServiceGenerateStreamRequest* request,
        grpc::ServerWriter<sentiric::llm::v1::LLMLocalServiceGenerateStreamResponse>* writer
    ) override;

private:
    std::shared_ptr<LLMEngine> engine_;
};
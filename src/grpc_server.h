#pragma once
#include "llm_engine.h"
#include <memory>
#include <grpcpp/grpcpp.h>
#include "local.grpc.pb.h" 

// gRPC servisinin implementasyonu
class GrpcServer final : public sentiric::llm::v1::LLMLocalService::Service {
public:
    explicit GrpcServer(std::shared_ptr<LLMEngine> engine);

    grpc::Status LocalGenerateStream(
        grpc::ServerContext* context,
        const sentiric::llm::v1::LocalGenerateStreamRequest* request,
        grpc::ServerWriter<sentiric::llm::v1::LocalGenerateStreamResponse>* writer
    ) override;

private:
    std::shared_ptr<LLMEngine> engine_;
};

// gRPC sunucusunu başlatan ve çalıştıran fonksiyon
void run_grpc_server(std::shared_ptr<LLMEngine> engine, const std::string& address);

// LLMEngine içinde olması gereken get_settings fonksiyonu deklarasyonu (config.h ve llm_engine.h arasında döngüsel bağımlılık olabilir, basitçe burada deklarasyon yapıyorum)
// NOT: Bu teknik olarak LLMEngine sınıfına eklenmelidir.
// LLM_Engine.h'deki LLMEngine sınıfına bu fonksiyonu ekliyorum.
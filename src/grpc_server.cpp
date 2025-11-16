#include "grpc_server.h"
#include "spdlog/spdlog.h"
#include <atomic>

GrpcServer::GrpcServer(std::shared_ptr<LLMEngine> engine) : engine_(std::move(engine)) {}

// KONTROL EDİN: Bu fonksiyonun adının ve imzasının ".h" dosyasıyla tam olarak eşleştiğinden emin olun.
grpc::Status GrpcServer::GenerateStream(
    grpc::ServerContext* context,
    const sentiric::llm::v1::LLMLocalServiceGenerateStreamRequest* request,
    grpc::ServerWriter<sentiric::llm::v1::LLMLocalServiceGenerateStreamResponse>* writer) {

    if (!engine_->is_model_loaded()) {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Model is not ready yet.");
    }
    if (request->user_prompt().empty()) {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "User prompt cannot be empty.");
    }
    
    spdlog::info("[gRPC] Stream started for user_prompt: '{}...'", request->user_prompt().substr(0, 50));
    
    try {
        engine_->generate_stream(
            *request,
            [&](const std::string& token) {
                sentiric::llm::v1::LLMLocalServiceGenerateStreamResponse response;
                response.set_token(token);
                if (!writer->Write(response)) { }
            },
            [&]() -> bool { return context->IsCancelled(); }
        );
    } catch (const std::exception& e) {
        spdlog::error("[gRPC] Unhandled exception: {}", e.what());
        return grpc::Status(grpc::StatusCode::INTERNAL, "An internal error occurred.");
    }

    if (context->IsCancelled()) {
        spdlog::warn("[gRPC] Stream cancelled by client.");
        return grpc::Status::CANCELLED;
    }

    sentiric::llm::v1::LLMLocalServiceGenerateStreamResponse final_response;
    auto* details = final_response.mutable_finish_details();
    details->set_finish_reason("stop");
    writer->Write(final_response);

    spdlog::info("[gRPC] Stream finished successfully.");
    return grpc::Status::OK;
}
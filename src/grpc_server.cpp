#include "grpc_server.h"
#include "spdlog/spdlog.h"
#include <atomic>

GrpcServer::GrpcServer(std::shared_ptr<LLMEngine> engine) : engine_(std::move(engine)) {}

grpc::Status GrpcServer::LocalGenerateStream(
    grpc::ServerContext* context,
    const sentiric::llm::v1::LocalGenerateStreamRequest* request,
    grpc::ServerWriter<sentiric::llm::v1::LocalGenerateStreamResponse>* writer) {

    if (!engine_->is_model_loaded()) {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Model is not ready yet.");
    }
    if (request->prompt().empty()) {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Prompt cannot be empty.");
    }
    
    spdlog::info("[gRPC] Stream started for prompt: '{}...'", request->prompt().substr(0, 50));
    
    try {
        engine_->generate_stream(
            *request,
            [&](const std::string& token) {
                sentiric::llm::v1::LocalGenerateStreamResponse response;
                response.set_token(token);
                if (!writer->Write(response)) {
                    // Client bağlantıyı kapattı. `should_stop_callback` bunu yakalayacak.
                }
            },
            [&]() -> bool {
                return context->IsCancelled();
            }
        );
    } catch (const std::exception& e) {
        spdlog::error("[gRPC] Unhandled exception: {}", e.what());
        return grpc::Status(grpc::StatusCode::INTERNAL, "An internal error occurred.");
    }

    if (context->IsCancelled()) {
        spdlog::warn("[gRPC] Stream cancelled by client.");
        return grpc::Status::CANCELLED;
    }

    // API tutarlılığı için FinishDetails gönder.
    sentiric::llm::v1::LocalGenerateStreamResponse final_response;
    auto* details = final_response.mutable_finish_details();
    details->set_finish_reason("stop"); // veya "length"
    writer->Write(final_response);

    spdlog::info("[gRPC] Stream finished successfully.");
    return grpc::Status::OK;
}

// run_grpc_server fonksiyonu bir önceki adımda src/main.cpp'ye taşındığı için
// burada artık bulunmuyor. Bu, kodun temiz halidir.
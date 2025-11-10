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
                    spdlog::warn("[gRPC] Write failed, client likely disconnected.");
                }
            },
            [&]() -> bool {
                return context->IsCancelled();
            }
        );
    } catch (const std::exception& e) {
        spdlog::error("[gRPC] Unhandled exception during stream generation: {}", e.what());
        return grpc::Status(grpc::StatusCode::INTERNAL, "An internal error occurred.");
    }

    if (context->IsCancelled()) {
        spdlog::warn("[gRPC] Stream cancelled by client.");
        return grpc::Status::CANCELLED;
    }

    spdlog::info("[gRPC] Stream finished successfully.");
    return grpc::Status::OK;
}

void run_grpc_server(std::shared_ptr<LLMEngine> engine, int port) {
    std::string address = "0.0.0.0:" + std::to_string(port);
    GrpcServer service(engine);
    grpc::ServerBuilder builder;
    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    if (!server) {
        spdlog::critical("gRPC server failed to start on {}", address);
        return;
    }
    spdlog::info("🚀 gRPC server listening on {}", address);
    server->Wait();
}
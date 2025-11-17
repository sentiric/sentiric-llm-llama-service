#include "grpc_server.h"
#include "spdlog/spdlog.h"
#include <atomic>

GrpcServer::GrpcServer(std::shared_ptr<LLMEngine> engine) : engine_(std::move(engine)) {}

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
    
    int32_t prompt_tokens = 0;
    int32_t completion_tokens = 0;
    std::string finish_reason = "stop";

    try {
        engine_->generate_stream(
            *request,
            [&](const std::string& token) {
                sentiric::llm::v1::LLMLocalServiceGenerateStreamResponse response;
                response.set_token(token); // string -> bytes otomatik conversion
                
                if (!writer->Write(response)) {
                    spdlog::warn("Failed to write response to stream");
                }
            },
            [&]() -> bool { return context->IsCancelled(); },
            prompt_tokens,
            completion_tokens
        );
    } catch (const std::exception& e) {
        spdlog::error("[gRPC] Unhandled exception: {}", e.what());
        finish_reason = "error";
    }

    if (context->IsCancelled()) {
        spdlog::warn("[gRPC] Stream cancelled by client.");
        finish_reason = "cancelled";
    }

    sentiric::llm::v1::LLMLocalServiceGenerateStreamResponse final_response;
    auto* details = final_response.mutable_finish_details();
    details->set_finish_reason(finish_reason);
    details->set_prompt_tokens(prompt_tokens);
    details->set_completion_tokens(completion_tokens);
    writer->Write(final_response);

    spdlog::info("[gRPC] Stream finished. Reason: '{}'. Tokens (Prompt/Completion): {}/{}", 
                 finish_reason, prompt_tokens, completion_tokens);
                 
    return grpc::Status::OK;
}
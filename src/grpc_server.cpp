// src/grpc_server.cpp
#include "grpc_server.h"
#include "spdlog/spdlog.h"
#include <atomic>

GrpcServer::GrpcServer(std::shared_ptr<LLMEngine> engine, AppMetrics& metrics)
    : engine_(std::move(engine)), metrics_(metrics) {}

grpc::Status GrpcServer::GenerateStream(
    grpc::ServerContext* context,
    const sentiric::llm::v1::LLMLocalServiceGenerateStreamRequest* request,
    grpc::ServerWriter<sentiric::llm::v1::LLMLocalServiceGenerateStreamResponse>* writer) {

    metrics_.requests_total.Increment();
    auto start_time = std::chrono::steady_clock::now();

    if (!engine_->is_model_loaded()) {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Model is not ready yet.");
    }
    if (request->user_prompt().empty()) {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "User prompt cannot be empty.");
    }
    
    auto batched_request = std::make_shared<BatchedRequest>();
    batched_request->request = *request;
    
    batched_request->on_token_callback = [&](const std::string& token) -> bool {
        sentiric::llm::v1::LLMLocalServiceGenerateStreamResponse response;
        response.set_token(token);
        return writer->Write(response);
    };

    batched_request->should_stop_callback = [&]() -> bool { 
        return context->IsCancelled(); 
    };

    try {
        if (engine_->is_batching_enabled()) {
            auto future = engine_->get_batcher()->add_request(batched_request);
            future.get(); // Batcher thread'inin bitmesini bekle
        } else {
            engine_->process_single_request(batched_request);
        }
    } catch (const std::exception& e) {
        spdlog::error("[gRPC] Unhandled exception during stream processing: {}", e.what());
        batched_request->finish_reason = "error";
    }

    if (context->IsCancelled() && batched_request->finish_reason != "error") {
        spdlog::warn("[gRPC] Stream cancelled by client.");
        batched_request->finish_reason = "cancelled";
    }

    sentiric::llm::v1::LLMLocalServiceGenerateStreamResponse final_response;
    auto* details = final_response.mutable_finish_details();
    details->set_finish_reason(batched_request->finish_reason);
    details->set_prompt_tokens(batched_request->prompt_tokens);
    details->set_completion_tokens(batched_request->completion_tokens);
    writer->Write(final_response);

    metrics_.tokens_generated_total.Increment(batched_request->completion_tokens);

    auto end_time = std::chrono::steady_clock::now();
    std::chrono::duration<double> latency = end_time - start_time;
    metrics_.request_latency.Observe(latency.count());

    spdlog::info("[gRPC] Stream finished. Reason: '{}'. Tokens (Prompt/Completion): {}/{}", 
                 batched_request->finish_reason, batched_request->prompt_tokens, batched_request->completion_tokens);
                 
    return grpc::Status::OK;
}

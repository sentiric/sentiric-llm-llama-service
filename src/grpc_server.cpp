#include "grpc_server.h"
#include "spdlog/spdlog.h"
#include <atomic>
#include <chrono>

GrpcServer::GrpcServer(std::shared_ptr<LLMEngine> engine, AppMetrics& metrics)
    : engine_(std::move(engine)), metrics_(metrics) {}

grpc::Status GrpcServer::GenerateStream(
    grpc::ServerContext* context,
    const sentiric::llm::v1::GenerateStreamRequest* request, 
    grpc::ServerWriter<sentiric::llm::v1::GenerateStreamResponse>* writer) { 

    metrics_.requests_total.Increment();
    auto start_time = std::chrono::steady_clock::now();

    std::string trace_id = "unknown";
    const auto& client_metadata = context->client_metadata();
    auto it = client_metadata.find("x-trace-id");
    if (it != client_metadata.end()) {
        trace_id = std::string(it->second.begin(), it->second.end());
    }
    
    // [YENİ] LoRA bilgisini logla
    spdlog::info("[gRPC][TraceID:{}] New GenerateStream request. LoRA: '{}'", 
                 trace_id, request->has_lora_adapter_id() ? request->lora_adapter_id() : "none");

    if (!engine_->is_model_loaded()) {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Model is not ready yet.");
    }
    if (request->user_prompt().empty()) {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "User prompt cannot be empty.");
    }
    
    auto batched_request = std::make_shared<BatchedRequest>();
    batched_request->request = *request;
    batched_request->creation_time = start_time;
    
    batched_request->on_token_callback = [batched_request, writer, trace_id](const std::string& token) -> bool {
        if (!batched_request->first_token_emitted.exchange(true)) {
            auto now = std::chrono::steady_clock::now();
            std::chrono::duration<double, std::milli> ttft = now - batched_request->creation_time;
            batched_request->ttft_ms = ttft.count();
            spdlog::debug("[gRPC][TraceID:{}] ⚡ TTFT: {:.2f} ms", trace_id, batched_request->ttft_ms.load());
        }

        sentiric::llm::v1::GenerateStreamResponse response; 
        response.set_token(token);
        return writer->Write(response);
    };

    batched_request->should_stop_callback = [&]() -> bool { 
        return context->IsCancelled(); 
    };

    try {
        if (engine_->is_batching_enabled()) {
            auto future = engine_->get_batcher()->add_request(batched_request);
            future.get();
        } else {
            engine_->process_single_request(batched_request);
        }
    } catch (const std::exception& e) {
        spdlog::error("Unhandled exception: {}", e.what());
        batched_request->finish_reason = "error";
    }

    sentiric::llm::v1::GenerateStreamResponse final_response; 
    auto* details = final_response.mutable_finish_details();
    details->set_finish_reason(batched_request->finish_reason);
    details->set_prompt_tokens(batched_request->prompt_tokens);
    details->set_completion_tokens(batched_request->completion_tokens);
    writer->Write(final_response);

    metrics_.tokens_generated_total.Increment(batched_request->completion_tokens);
    
    auto end_time = std::chrono::steady_clock::now();
    std::chrono::duration<double> latency = end_time - start_time;
    metrics_.request_latency.Observe(latency.count());

    spdlog::info("[gRPC][TraceID:{}] Completed. Tokens: {}/{}, TTFT: {:.2f}ms, Total: {:.2f}s", 
        trace_id, batched_request->prompt_tokens, batched_request->completion_tokens,
        batched_request->ttft_ms.load(), latency.count());

    return grpc::Status::OK;
}
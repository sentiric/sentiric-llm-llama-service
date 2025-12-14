#include "grpc_server.h"
#include "spdlog/spdlog.h"
#include <atomic>
#include <algorithm>
#include <vector>

// Yardımcı Fonksiyon: Token Temizleme (ChatController'daki mantığın aynısı)
// Model bazen 0x00 (Null) veya kontrol karakterleri üretebilir. Bunlar gRPC stream'i bozmaz
// ama CLI ve Client tarafında işlemeyi zorlaştırır.
std::string sanitize_token_grpc(const std::string& input) {
    std::string clean = input;
    clean.erase(std::remove_if(clean.begin(), clean.end(), [](unsigned char c) {
        // 0-31 arası kontrol karakterlerini sil (Tab, Newline ve Carriage Return hariç)
        // Ve 127 (DEL) karakterini sil.
        return (c < 32 && c != 9 && c != 10 && c != 13) || c == 127;
    }), clean.end());
    return clean;
}

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
    
    spdlog::info("[gRPC][TraceID:{}] New GenerateStream request received.", trace_id);

    if (!engine_->is_model_loaded()) {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Model is not ready yet.");
    }
    if (request->user_prompt().empty()) {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "User prompt cannot be empty.");
    }
    
    auto batched_request = std::make_shared<BatchedRequest>();
    batched_request->request = *request;
    
    // Callback içinde sanitasyon yapıyoruz
    batched_request->on_token_callback = [&](const std::string& raw_token) -> bool {
        std::string clean_token = sanitize_token_grpc(raw_token);
        
        // Eğer token sadece silinen karakterlerden oluşuyorsa (örn: sadece NULL byte),
        // boş string göndermeye gerek yok, true dönüp devam et.
        if (clean_token.empty()) {
            return true; 
        }

        sentiric::llm::v1::GenerateStreamResponse response; 
        response.set_token(clean_token);
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

    return grpc::Status::OK;
}
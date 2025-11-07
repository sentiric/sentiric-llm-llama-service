#include "grpc_server.h"
#include "spdlog/spdlog.h"

GrpcServer::GrpcServer(std::shared_ptr<LLMEngine> engine) : engine_(std::move(engine)) {}

grpc::Status GrpcServer::LocalGenerateStream(
    grpc::ServerContext* context,
    const sentiric::llm::v1::LocalGenerateStreamRequest* request,
    grpc::ServerWriter<sentiric::llm::v1::LocalGenerateStreamResponse>* writer) {

    if (!engine_->is_model_loaded()) {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Model is not ready yet.");
    }

    const auto& prompt = request->prompt();
    if (prompt.empty() || prompt.find_first_not_of(" \t\n\v\f\r") == std::string::npos) {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Prompt cannot be empty.");
    }
    
    // Konfigürasyondan varsayılan ayarları al
    const Settings& settings = engine_->get_settings();

    // İstekten gelen veya varsayılan sampling parametrelerini belirle
    float temp = request->has_temperature() ? request->temperature() : settings.default_temperature;
    int top_k = request->has_top_k() ? request->top_k() : settings.default_top_k;
    float top_p = request->has_top_p() ? request->top_p() : settings.default_top_p;

    spdlog::info("[gRPC] Stream started for prompt: '{}...' (T:{:.2f}, K:{}, P:{:.2f})", 
        prompt.substr(0, 40), temp, top_k, top_p);

    try {
        engine_->generate_stream(prompt,
            [&](const std::string& token) {
                sentiric::llm::v1::LocalGenerateStreamResponse response;
                response.set_token(token);
                // Eğer Writer::Write başarısız olursa (istemci bağlantıyı kapattı)
                if (!writer->Write(response)) {
                    // İstemci iptal etti, akışı durdur
                    spdlog::warn("[gRPC] Client cancelled the stream or connection failed.");
                    throw std::runtime_error("Client cancelled");
                }
            },
            temp, top_k, top_p // Yeni parametreler
        );
    } catch (const std::runtime_error& e) {
        if (std::string(e.what()) == "Client cancelled") {
             return grpc::Status(grpc::StatusCode::CANCELLED, "Stream cancelled by client.");
        }
        spdlog::error("[gRPC] Unhandled exception during stream generation: {}", e.what());
        return grpc::Status(grpc::StatusCode::INTERNAL, "An internal error occurred during stream generation.");
    } catch (const std::exception& e) {
        spdlog::error("[gRPC] Unhandled exception during stream generation: {}", e.what());
        return grpc::Status(grpc::StatusCode::INTERNAL, "An internal error occurred during stream generation.");
    }


    spdlog::info("[gRPC] Stream finished successfully.");
    return grpc::Status::OK;
}

void run_grpc_server(std::shared_ptr<LLMEngine> engine, const std::string& address) {
    GrpcServer service(engine);
    grpc::ServerBuilder builder;
    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    spdlog::info("🚀 gRPC server listening on {}", address);
    server->Wait();
}

// LLMEngine'a settings erişim fonksiyonu eklenmeli
const Settings& LLMEngine::get_settings() const {
    return settings_;
}
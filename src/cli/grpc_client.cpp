#include "grpc_client.h"
#include "spdlog/spdlog.h"
#include <grpcpp/grpcpp.h>

namespace sentiric_llm_cli {

GRPCClient::GRPCClient(const std::string& endpoint) 
    : endpoint_(endpoint) {
}

GRPCClient::~GRPCClient() {
}

bool GRPCClient::connect() {
    try {
        channel_ = grpc::CreateChannel(endpoint_, grpc::InsecureChannelCredentials());
        stub_ = sentiric::llm::v1::LLMLocalService::NewStub(channel_);
        
        // Bağlantı testi
        grpc::ClientContext context;
        context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));
        
        sentiric::llm::v1::LocalGenerateStreamRequest request;
        request.set_prompt("test");
        auto* params = request.mutable_params();
        params->set_max_new_tokens(1);
        
        auto reader = stub_->LocalGenerateStream(&context, request);
        sentiric::llm::v1::LocalGenerateStreamResponse response;
        
        // Hemen iptal et - sadece bağlantı testi
        context.TryCancel();
        
        spdlog::info("✅ GRPC bağlantısı başarılı: {}", endpoint_);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("❌ GRPC bağlantı hatası: {}", e.what());
        return false;
    }
}

bool GRPCClient::is_connected() const {
    return channel_ && channel_->GetState(false) == GRPC_CHANNEL_READY;
}

bool GRPCClient::generate_stream(const std::string& prompt,
                                std::function<void(const std::string&)> on_token,
                                float temperature,
                                int max_tokens) {
    if (!is_connected()) {
        spdlog::error("GRPC bağlantısı yok");
        return false;
    }
    
    try {
        sentiric::llm::v1::LocalGenerateStreamRequest request;
        request.set_prompt(prompt);
        
        auto* params = request.mutable_params();
        params->set_temperature(temperature);
        params->set_max_new_tokens(max_tokens);
        params->set_top_k(40);
        params->set_top_p(0.95f);
        
        grpc::ClientContext context;
        context.set_deadline(std::chrono::system_clock::now() + 
                           std::chrono::seconds(timeout_seconds_));
        
        auto reader = stub_->LocalGenerateStream(&context, request);
        sentiric::llm::v1::LocalGenerateStreamResponse response;
        
        std::cout << "\n🤖 AI Yanıtı: ";
        while (reader->Read(&response)) {
            if (response.has_token()) {
                std::string token = response.token();
                std::cout << token << std::flush;
                if (on_token) {
                    on_token(token);
                }
            }
        }
        std::cout << std::endl;
        
        auto status = reader->Finish();
        if (!status.ok()) {
            spdlog::warn("GRPC stream tamamlandı: {}", status.error_message());
        }
        
        return true;
    } catch (const std::exception& e) {
        spdlog::error("GRPC generation hatası: {}", e.what());
        return false;
    }
}

void GRPCClient::set_timeout(int seconds) {
    timeout_seconds_ = seconds;
}

} // namespace sentiric_llm_cli
#include "grpc_client.h"
#include "spdlog/spdlog.h"
#include "fmt/format.h" // YENİ: fmt kütüphanesini dahil et
#include <grpcpp/grpcpp.h>
#include <string_view>

// YENİ: spdlog'a grpc::StatusCode'u nasıl formatlayacağını öğreten uzmanlaşma
template <>
struct fmt::formatter<grpc::StatusCode> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const grpc::StatusCode& code, FormatContext& ctx) const {
        std::string_view name = "UNKNOWN";
        switch (code) {
            case grpc::StatusCode::OK: name = "OK"; break;
            case grpc::StatusCode::CANCELLED: name = "CANCELLED"; break;
            case grpc::StatusCode::UNKNOWN: name = "UNKNOWN"; break;
            case grpc::StatusCode::INVALID_ARGUMENT: name = "INVALID_ARGUMENT"; break;
            case grpc::StatusCode::DEADLINE_EXCEEDED: name = "DEADLINE_EXCEEDED"; break;
            case grpc::StatusCode::NOT_FOUND: name = "NOT_FOUND"; break;
            case grpc::StatusCode::ALREADY_EXISTS: name = "ALREADY_EXISTS"; break;
            case grpc::StatusCode::PERMISSION_DENIED: name = "PERMISSION_DENIED"; break;
            case grpc::StatusCode::UNAUTHENTICATED: name = "UNAUTHENTICATED"; break;
            case grpc::StatusCode::RESOURCE_EXHAUSTED: name = "RESOURCE_EXHAUSTED"; break;
            case grpc::StatusCode::FAILED_PRECONDITION: name = "FAILED_PRECONDITION"; break;
            case grpc::StatusCode::ABORTED: name = "ABORTED"; break;
            case grpc::StatusCode::OUT_OF_RANGE: name = "OUT_OF_RANGE"; break;
            case grpc::StatusCode::UNIMPLEMENTED: name = "UNIMPLEMENTED"; break;
            case grpc::StatusCode::INTERNAL: name = "INTERNAL"; break;
            case grpc::StatusCode::UNAVAILABLE: name = "UNAVAILABLE"; break;
            case grpc::StatusCode::DATA_LOSS: name = "DATA_LOSS"; break;
        }
        return fmt::format_to(ctx.out(), "{} ({})", name, static_cast<int>(code));
    }
};


namespace sentiric_llm_cli {

GRPCClient::GRPCClient(const std::string& endpoint) 
    : endpoint_(endpoint) {
}

GRPCClient::~GRPCClient() {
}

bool GRPCClient::is_connected() const {
    if (!channel_) return false;
    return channel_->WaitForConnected(std::chrono::system_clock::now() + std::chrono::seconds(1));
}

bool GRPCClient::generate_stream(const std::string& prompt,
                                std::function<void(const std::string&)> on_token,
                                float temperature,
                                int max_tokens) {

    if (!stub_) {
        channel_ = grpc::CreateChannel(endpoint_, grpc::InsecureChannelCredentials());
        stub_ = sentiric::llm::v1::LLMLocalService::NewStub(channel_);
    }

    if (!is_connected()) {
        spdlog::error("gRPC connection to {} failed or timed out.", endpoint_);
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
            if (status.error_code() != grpc::StatusCode::CANCELLED) {
                 // DÜZELTME: Artık status.error_code() formatlanabilir.
                 spdlog::warn("GRPC stream finished with status [{}]: {}", status.error_code(), status.error_message());
            }
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
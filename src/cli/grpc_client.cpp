#include "grpc_client.h"
#include "spdlog/spdlog.h"
#include "fmt/format.h"
#include <grpcpp/grpcpp.h>
#include <string_view>
#include <grpcpp/security/credentials.h>
#include <fstream>
#include <sstream>

// fmt::formatter uzmanlaşması
template <>
struct fmt::formatter<grpc::StatusCode> {
    constexpr auto parse(fmt::format_parse_context& ctx) -> decltype(ctx.begin()) {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const grpc::StatusCode& code, FormatContext& ctx) const -> decltype(ctx.out()) {
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
            default: break;
        }
        return fmt::format_to(ctx.out(), "{} ({})", name, static_cast<int>(code));
    }
};

namespace sentiric_llm_cli {

// Yardımcı fonksiyon
std::string read_file_client(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("File could not be opened for client: " + filepath);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

GRPCClient::GRPCClient(const std::string& endpoint) 
    : endpoint_(endpoint) {}

GRPCClient::~GRPCClient() {}

void GRPCClient::ensure_channel_is_ready() {
    if (stub_) return;
    
    std::shared_ptr<grpc::ChannelCredentials> creds;
    
    try {
        const char* ca_path = std::getenv("GRPC_TLS_CA_PATH");
        const char* cert_path = std::getenv("LLM_LLAMA_SERVICE_CERT_PATH");
        const char* key_path = std::getenv("LLM_LLAMA_SERVICE_KEY_PATH");

        spdlog::debug("TLS Config - CA: {}, CERT: {}, KEY: {}", 
                     ca_path ? "SET" : "NOT SET",
                     cert_path ? "SET" : "NOT SET", 
                     key_path ? "SET" : "NOT SET");

        if (!ca_path || !cert_path || !key_path) {
            spdlog::warn("gRPC TLS environment variables not set for CLI. Using insecure credentials.");
            creds = grpc::InsecureChannelCredentials();
        } else {
            if (access(ca_path, R_OK) != 0 || access(cert_path, R_OK) != 0 || access(key_path, R_OK) != 0) {
                spdlog::warn("TLS certificate files not accessible. Using insecure credentials.");
                creds = grpc::InsecureChannelCredentials();
            } else {
                spdlog::info("Loading TLS credentials for gRPC client...");
                std::string root_ca = read_file_client(ca_path);
                std::string client_cert = read_file_client(cert_path);
                std::string client_key = read_file_client(key_path);

                grpc::SslCredentialsOptions ssl_opts;
                ssl_opts.pem_root_certs = root_ca;
                ssl_opts.pem_private_key = client_key;
                ssl_opts.pem_cert_chain = client_cert;
                creds = grpc::SslCredentials(ssl_opts);
                spdlog::info("gRPC client configured to use mTLS.");
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Failed to create channel credentials: {}. Using insecure.", e.what());
        creds = grpc::InsecureChannelCredentials();
    }
    
    channel_ = grpc::CreateChannel(endpoint_, creds);
    // DÜZELTME: LlamaService
    stub_ = sentiric::llm::v1::LlamaService::NewStub(channel_);
}

bool GRPCClient::is_connected() {
    ensure_channel_is_ready();
    return channel_->WaitForConnected(
        gpr_time_add(gpr_now(GPR_CLOCK_REALTIME), 
        gpr_time_from_seconds(2, GPR_TIMESPAN))
    );
}

// DÜZELTME: Parametre tipi GenerateStreamRequest
bool GRPCClient::generate_stream(
    const sentiric::llm::v1::GenerateStreamRequest& request,
    std::function<void(const std::string&)> on_token) {
    
    ensure_channel_is_ready();

    if (!is_connected()) {
        spdlog::error("gRPC connection to {} failed or timed out.", endpoint_);
        return false;
    }
    
    try {
        grpc::ClientContext context;
        context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(timeout_seconds_));
        
        auto reader = stub_->GenerateStream(&context, request);
        
        // DÜZELTME: Yanıt tipi GenerateStreamResponse
        sentiric::llm::v1::GenerateStreamResponse response;
        
        while (reader->Read(&response)) {
            if (response.has_token()) {
                if (on_token) {
                    on_token(response.token());
                }
            }
            if (response.has_finish_details()) {
                const auto& details = response.finish_details();
                spdlog::info("\n[Stream Bitti: Neden='{}', Prompt/Completion Tokenları={}/{}]", 
                             details.finish_reason(), details.prompt_tokens(), details.completion_tokens());
            }
        }
        
        auto status = reader->Finish();
        if (!status.ok() && status.error_code() != grpc::StatusCode::CANCELLED) {
             spdlog::warn("GRPC stream finished with status [{}]: {}", status.error_code(), status.error_message());
        }
        
        return status.ok() || status.error_code() == grpc::StatusCode::CANCELLED;
    } catch (const std::exception& e) {
        spdlog::error("gRPC generation error: {}", e.what());
        return false;
    }
}

void GRPCClient::set_timeout(int seconds) {
    timeout_seconds_ = seconds;
}

} // namespace sentiric_llm_cli
#include "grpc_client.h"
#include "spdlog/spdlog.h"
#include "fmt/format.h"
#include <grpcpp/grpcpp.h>
#include <string_view>
#include <grpcpp/security/credentials.h>
#include <fstream>
#include <sstream>
#include <unistd.h>

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
            // ... (diğer case'ler aynı) ...
            default: break;
        }
        return fmt::format_to(ctx.out(), "{} ({})", name, static_cast<int>(code));
    }
};

namespace sentiric_llm_cli {

std::string read_file_client(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) { throw std::runtime_error("File could not be opened for client: " + filepath); }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

GRPCClient::GRPCClient(const std::string& endpoint) 
    : endpoint_(endpoint) {}

GRPCClient::~GRPCClient() {}

void GRPCClient::cancel_stream() {
    std::lock_guard<std::mutex> lock(context_mutex_);
    if (cancellable_context_) {
        cancellable_context_->TryCancel();
    }
}

void GRPCClient::ensure_channel_is_ready() {
    if (stub_) return;
    std::shared_ptr<grpc::ChannelCredentials> creds;
    try {
        const char* ca = std::getenv("GRPC_TLS_CA_PATH"), *cert = std::getenv("LLM_LLAMA_SERVICE_CERT_PATH"), *key = std::getenv("LLM_LLAMA_SERVICE_KEY_PATH");
        if (!ca || !cert || !key || access(ca, R_OK) != 0 || access(cert, R_OK) != 0 || access(key, R_OK) != 0) {
            creds = grpc::InsecureChannelCredentials();
        } else {
            grpc::SslCredentialsOptions opts = {read_file_client(ca), read_file_client(key), read_file_client(cert)};
            creds = grpc::SslCredentials(opts);
        }
    } catch (...) { creds = grpc::InsecureChannelCredentials(); }
    channel_ = grpc::CreateChannel(endpoint_, creds);
    stub_ = sentiric::llm::v1::LlamaService::NewStub(channel_);
}

bool GRPCClient::is_connected() {
    ensure_channel_is_ready();
    return channel_->WaitForConnected(gpr_time_add(gpr_now(GPR_CLOCK_REALTIME), gpr_time_from_seconds(2, GPR_TIMESPAN)));
}

bool GRPCClient::generate_stream_with_cancellation(
    const sentiric::llm::v1::GenerateStreamRequest& request,
    std::function<void(const std::string&)> on_token) {
    ensure_channel_is_ready();
    if (!is_connected()) return false;
    {
        std::lock_guard<std::mutex> lock(context_mutex_);
        cancellable_context_ = std::make_shared<grpc::ClientContext>();
    }
    cancellable_context_->set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(timeout_seconds_));
    auto reader = stub_->GenerateStream(cancellable_context_.get(), request);
    sentiric::llm::v1::GenerateStreamResponse response;
    while (reader->Read(&response)) {
        if (response.has_token() && on_token) on_token(response.token());
    }
    auto status = reader->Finish();
    if (!status.ok() && status.error_code() != grpc::StatusCode::CANCELLED) {
         spdlog::warn("gRPC stream finished with status [{}]: {}", status.error_code(), status.error_message());
    }
    return status.ok() || status.error_code() == grpc::StatusCode::CANCELLED;
}

bool GRPCClient::generate_stream(
    const sentiric::llm::v1::GenerateStreamRequest& request,
    std::function<void(const std::string&)> on_token) {
    ensure_channel_is_ready();
    if (!is_connected()) return false;
    grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(timeout_seconds_));
    context.AddMetadata("x-trace-id", "cli-test-" + std::to_string(std::time(nullptr)));
    auto reader = stub_->GenerateStream(&context, request);
    sentiric::llm::v1::GenerateStreamResponse response;
    while (reader->Read(&response)) {
        if (response.has_token() && on_token) on_token(response.token());
        if (response.has_finish_details()) {
            const auto& d = response.finish_details();
            spdlog::info("\n[Stream Bitti: Neden='{}', Prompt/Completion Tokenları={}/{}]", d.finish_reason(), d.prompt_tokens(), d.completion_tokens());
        }
    }
    auto status = reader->Finish();
    if (!status.ok() && status.error_code() != grpc::StatusCode::CANCELLED) {
         spdlog::warn("gRPC stream finished with status [{}]: {}", status.error_code(), status.error_message());
    }
    return status.ok() || status.error_code() == grpc::StatusCode::CANCELLED;
}

void GRPCClient::set_timeout(int seconds) {
    timeout_seconds_ = seconds;
}

} // namespace sentiric_llm_cli
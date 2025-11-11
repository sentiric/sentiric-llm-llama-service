#include "cli_client.h"
#include "grpc_client.h"
#include "http_client.h"
#include "benchmark.h"
#include "health_check.h"
#include "spdlog/spdlog.h"

namespace sentiric_llm_cli {

// Constructor
CLIClient::CLIClient(const std::string& grpc_endpoint, const std::string& http_endpoint)
    : grpc_endpoint_(grpc_endpoint), http_endpoint_(http_endpoint) {
    grpc_client_ = std::make_unique<GRPCClient>(grpc_endpoint_);
    http_client_ = std::make_unique<HTTPClient>(http_endpoint_);
}

// Destructor (unique_ptr'lar için otomatik temizlik yeterli)
CLIClient::~CLIClient() = default;

// gRPC İşlemleri
bool CLIClient::generate_stream(const std::string& prompt, 
                                std::function<void(const std::string&)> on_token,
                                float temperature,
                                int max_tokens) {
    if (!grpc_client_) return false;
    return grpc_client_->generate_stream(prompt, on_token, temperature, max_tokens);
}

// HTTP İşlemleri
bool CLIClient::health_check() {
    if (!http_client_) return false;
    auto status = http_client_->check_health();
    return status.status == "healthy" && status.model_ready;
}

std::string CLIClient::get_health_status() {
    if (!http_client_) return "error";
    return http_client_->check_health().status;
}

bool CLIClient::http_generate(const std::string& prompt, std::string& response) {
    if (!http_client_) return false;
    return http_client_->generate(prompt, response);
}

// Utility
void CLIClient::set_timeout(int seconds) {
    timeout_seconds_ = seconds;
    if (grpc_client_) grpc_client_->set_timeout(seconds);
    if (http_client_) http_client_->set_timeout(seconds);
}

bool CLIClient::is_connected() const {
    if (!grpc_client_) return false;
    return grpc_client_->is_connected();
}

} // namespace sentiric_llm_cli
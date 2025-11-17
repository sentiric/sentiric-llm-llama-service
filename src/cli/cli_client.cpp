#include "cli_client.h"
#include "grpc_client.h"
#include "http_client.h"
#include "spdlog/spdlog.h"
#include <stdexcept>

namespace sentiric_llm_cli {

CLIClient::CLIClient(const std::string& grpc_endpoint, const std::string& http_endpoint)
    : grpc_endpoint_(grpc_endpoint), 
      http_endpoint_(http_endpoint),
      timeout_seconds_(120) {
    
    grpc_client_ = std::make_unique<GRPCClient>(grpc_endpoint_);
    http_client_ = std::make_unique<HTTPClient>(http_endpoint_);
}

CLIClient::~CLIClient() = default;

bool CLIClient::generate_stream(const std::string& prompt, 
                               const std::function<void(const std::string&)>& on_token) {
    
    // Basit bir request oluştur - benchmark için
    sentiric::llm::v1::LLMLocalServiceGenerateStreamRequest request;
    request.set_system_prompt("You are a helpful assistant.");
    request.set_user_prompt(prompt);
    
    return grpc_client_->generate_stream(request, on_token);
}

bool CLIClient::health_check() {
    auto status = http_client_->check_health();
    return status.status == "healthy" && status.model_ready;
}

std::string CLIClient::get_health_status() {
    auto status = http_client_->check_health();
    return status.status;
}

void CLIClient::set_timeout(int seconds) {
    timeout_seconds_ = seconds;
    if (grpc_client_) grpc_client_->set_timeout(seconds);
    if (http_client_) http_client_->set_timeout(seconds);
}

bool CLIClient::is_connected() const {
    return http_client_->is_connected();
}

} // namespace sentiric_llm_cli
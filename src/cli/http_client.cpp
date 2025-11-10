#include "http_client.h"
#include "spdlog/spdlog.h"

namespace sentiric_llm_cli {

HTTPClient::HTTPClient(const std::string& endpoint) 
    : endpoint_(endpoint) {
    size_t colon_pos = endpoint.find(':');
    std::string host = endpoint.substr(0, colon_pos);
    int port = std::stoi(endpoint.substr(colon_pos + 1));
    client_ = std::make_unique<httplib::Client>(host, port);
    client_->set_connection_timeout(timeout_seconds_, 0);
    client_->set_read_timeout(timeout_seconds_, 0);
    client_->set_write_timeout(timeout_seconds_, 0);
}

bool HTTPClient::connect() {
    auto res = client_->Get("/health");
    return res && res->status == 200;
}

bool HTTPClient::is_connected() const {
    // Yeni const uyumlu implementasyon
    try {
        size_t colon_pos = endpoint_.find(':');
        std::string host = endpoint_.substr(0, colon_pos);
        int port = std::stoi(endpoint_.substr(colon_pos + 1));
        
        auto temp_client = std::make_unique<httplib::Client>(host, port);
        temp_client->set_connection_timeout(5, 0);
        auto res = temp_client->Get("/health");
        return res && res->status == 200;
    } catch (...) {
        return false;
    }
}

HTTPClient::HealthStatus HTTPClient::check_health() {
    HealthStatus status;
    
    try {
        auto res = client_->Get("/health");
        if (res && res->status == 200) {
            auto json = nlohmann::json::parse(res->body);
            status.status = json.value("status", "unknown");
            status.model_ready = json.value("model_ready", false);
            status.engine = json.value("engine", "unknown");
            status.timestamp = json.value("timestamp", "");
        }
    } catch (const std::exception& e) {
        spdlog::error("HTTP health check hatası: {}", e.what());
        status.status = "error";
    }
    
    return status;
}

bool HTTPClient::generate(const std::string& prompt, std::string& response) {
    try {
        nlohmann::json request_json = {
            {"prompt", prompt},
            {"max_tokens", 100},
            {"temperature", 0.7}
        };
        
        auto res = client_->Post("/generate", request_json.dump(), "application/json");
        if (res && res->status == 200) {
            auto json = nlohmann::json::parse(res->body);
            response = json.value("response", "");
            return true;
        }
    } catch (const std::exception& e) {
        spdlog::error("HTTP generation hatası: {}", e.what());
    }
    
    return false;
}

void HTTPClient::set_timeout(int seconds) {
    timeout_seconds_ = seconds;
    client_->set_connection_timeout(timeout_seconds_, 0);
    client_->set_read_timeout(timeout_seconds_, 0);
    client_->set_write_timeout(timeout_seconds_, 0);
}

} // namespace sentiric_llm_cli
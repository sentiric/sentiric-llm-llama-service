#pragma once

#include <string>
#include <memory>
#include "httplib.h"
#include "nlohmann/json.hpp"

namespace sentiric_llm_cli {

class HTTPClient {
public:
    HTTPClient(const std::string& endpoint);
    
    bool connect();
    bool is_connected() const;
    
    struct HealthStatus {
        std::string status;
        bool model_ready;
        std::string engine;
        std::string timestamp;
    };
    
    HealthStatus check_health();
    bool generate(const std::string& prompt, std::string& response);
    void set_timeout(int seconds);

private:
    std::string endpoint_;
    int timeout_seconds_ = 10;
    std::unique_ptr<httplib::Client> client_;
};

} // namespace sentiric_llm_cli
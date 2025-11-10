#pragma once

#include <string>
#include "cli_client.h"

namespace sentiric_llm_cli {

class HealthChecker {
public:
    HealthChecker(const std::string& grpc_endpoint, const std::string& http_endpoint);
    
    struct SystemStatus {
        bool grpc_healthy;
        bool http_healthy;
        bool model_ready;
        std::string overall_status;
        int response_time_ms;
    };
    
    SystemStatus check_system();
    bool wait_for_ready(int max_wait_seconds = 300, int check_interval = 5);
    void print_detailed_status();

private:
    std::unique_ptr<CLIClient> client_;
};

} // namespace sentiric_llm_cli
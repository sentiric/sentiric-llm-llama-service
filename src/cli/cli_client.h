#pragma once

#include <string>
#include <memory>
#include <functional>

namespace sentiric_llm_cli {

class CLIClient {
public:
    CLIClient(const std::string& grpc_endpoint = "localhost:16061", 
              const std::string& http_endpoint = "localhost:16060");
    
    // GRPC İşlemleri
    bool generate_stream(const std::string& prompt, 
                        std::function<void(const std::string&)> on_token = nullptr,
                        float temperature = 0.8f,
                        int max_tokens = 2048);
    
    // HTTP İşlemleri
    bool health_check();
    std::string get_health_status();
    bool http_generate(const std::string& prompt, std::string& response);
    
    // Benchmark
    void run_benchmark(int iterations = 10, const std::string& test_prompt = "Test prompt");
    
    // Utility
    void set_timeout(int seconds);
    bool is_connected() const;

private:
    std::string grpc_endpoint_;
    std::string http_endpoint_;
    int timeout_seconds_ = 30;
};

} // namespace sentiric_llm_cli
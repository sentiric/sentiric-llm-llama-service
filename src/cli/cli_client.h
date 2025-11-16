#pragma once

#include <string>
#include <memory>
#include <functional>
#include "sentiric/llm/v1/local.pb.h" // Kontratları dahil et

namespace sentiric_llm_cli {

class GRPCClient;
class HTTPClient;

class CLIClient {
public:
    CLIClient(const std::string& grpc_endpoint = "localhost:16071", 
              const std::string& http_endpoint = "localhost:16070");
    ~CLIClient();
    
    // GÜNCELLENDİ: Bu fonksiyon artık sadece basit string prompt kabul eder.
    // Benchmark gibi basit testler için kullanılır.
    bool generate_stream(const std::string& prompt, 
                         const std::function<void(const std::string&)>& on_token);
    
    // HTTP İşlemleri
    bool health_check();
    std::string get_health_status();
    
    // Utility
    void set_timeout(int seconds);
    bool is_connected() const;

private:
    std::string grpc_endpoint_;
    std::string http_endpoint_;
    int timeout_seconds_ = 120;

    std::unique_ptr<GRPCClient> grpc_client_;
    std::unique_ptr<HTTPClient> http_client_;
};

} // namespace sentiric_llm_cli
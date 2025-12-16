#pragma once

#include <string>
#include <vector>
#include <chrono>
#include "cli_client.h"

namespace sentiric_llm_cli {

struct BenchmarkResult {
    int total_requests;
    int successful_requests;
    double average_response_time_ms;
    double tokens_per_second;
    double error_rate;
    long long total_tokens_generated;
    std::chrono::seconds total_duration;
    std::vector<double> response_times;
};

class Benchmark {
public:
    Benchmark(const std::string& grpc_endpoint);
    
    BenchmarkResult run_performance_test(int iterations = 10, 
                                       const std::string& prompt = "Test prompt");
    
    BenchmarkResult run_concurrent_test(int concurrent_connections = 5,
                                      int requests_per_connection = 10);
    
    void run_interrupt_test(const std::string& initial_prompt, const std::string& interrupt_prompt);

    void generate_report(const BenchmarkResult& result, const std::string& filename = "");

private:
    std::string grpc_endpoint_;
    std::unique_ptr<CLIClient> client_;
};

} // namespace sentiric_llm_cli
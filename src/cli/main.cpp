#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include "spdlog/spdlog.h"
#include "cli_client.h"
#include "health_check.h"
#include "benchmark.h"

void print_usage() {
    std::cout << R"(
🧠 Sentiric LLM CLI Aracı v1.0

Kullanım:
  llm_cli <komut> [seçenekler]

Komutlar:
  generate <prompt>    - Metin üret (GRPC stream)
  health               - Sistem sağlık durumu
  monitor              - Gerçek zamanlı sistem izleme
  benchmark            - Performans testi çalıştır
  wait-for-ready       - Servis hazır olana kadar bekle

Seçenekler:
  --grpc-endpoint      - GRPC endpoint (varsayılan: localhost:16071)
  --http-endpoint      - HTTP endpoint (varsayılan: localhost:16070)
  --timeout            - Zaman aşımı süresi (saniye)
  --iterations         - Benchmark iterasyon sayısı
  --output             - Çıktı dosyası

Örnekler:
  llm_cli generate "Türkiye'nin başkenti neresidir?"
  llm_cli health --http-endpoint 192.168.1.100:16070
  llm_cli benchmark --iterations 50
  llm_cli wait-for-ready --timeout 120
)";
}

int main(int argc, char** argv) {
    spdlog::set_level(spdlog::level::info);
    
    if (argc < 2) {
        print_usage();
        return 1;
    }
    
    std::string command = argv[1];
    std::map<std::string, std::string> options;
    
    // Seçenekleri parse et
    for (int i = 2; i < argc; i++) {
        std::string arg = argv[i];
        if (arg.substr(0, 2) == "--") {
            std::string key = arg.substr(2);
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                options[key] = argv[++i];
            } else {
                options[key] = "true";
            }
        }
    }
    
    std::string grpc_endpoint = options.count("grpc-endpoint") ? 
                               options["grpc-endpoint"] : "localhost:16071";
    std::string http_endpoint = options.count("http-endpoint") ? 
                               options["http-endpoint"] : "localhost:16070";
    
    try {
        if (command == "generate" && argc > 2) {
            std::string prompt = argv[2];
            sentiric_llm_cli::CLIClient client(grpc_endpoint, http_endpoint);
            
            if (options.count("timeout")) {
                client.set_timeout(std::stoi(options["timeout"]));
            }
            
            std::cout << "👤 Kullanıcı: " << prompt << "\n";
            bool success = client.generate_stream(prompt);
            
            if (!success) {
                spdlog::error("Generation başarısız");
                return 1;
            }
            
        } else if (command == "health") {
            sentiric_llm_cli::HealthChecker checker(grpc_endpoint, http_endpoint);
            checker.print_detailed_status();
            
        } else if (command == "wait-for-ready") {
            int timeout = options.count("timeout") ? std::stoi(options["timeout"]) : 300;
            sentiric_llm_cli::HealthChecker checker(grpc_endpoint, http_endpoint);
            
            if (!checker.wait_for_ready(timeout)) {
                return 1;
            }
            
        } else if (command == "benchmark") {
            int iterations = options.count("iterations") ? std::stoi(options["iterations"]) : 10;
            std::string output_file = options.count("output") ? options["output"] : "";
            
            sentiric_llm_cli::Benchmark benchmark(grpc_endpoint);
            auto result = benchmark.run_performance_test(iterations);
            benchmark.generate_report(result, output_file);
            
        } else if (command == "monitor") {
            spdlog::info("Gerçek zamanlı izleme başlatılıyor... (Ctrl+C ile durdur)");
            sentiric_llm_cli::HealthChecker checker(grpc_endpoint, http_endpoint);
            
            while (true) {
                checker.print_detailed_status();
                std::this_thread::sleep_for(std::chrono::seconds(10));
            }
            
        } else {
            std::cerr << "❌ Geçersiz komut: " << command << "\n";
            print_usage();
            return 1;
        }
        
    } catch (const std::exception& e) {
        spdlog::critical("CLI hatası: {}", e.what());
        return 1;
    }
    
    return 0;
}
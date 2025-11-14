#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <algorithm>
#include "spdlog/spdlog.h"
#include "cli_client.h"
#include "health_check.h"
#include "benchmark.h"

void print_usage() {
    std::cout << R"(
🧠 Sentiric LLM CLI Aracı v1.1

Kullanım:
  llm_cli [seçenekler] <komut> [argümanlar]

Komutlar:
  generate <prompt>    - Metin üret (GRPC stream)
  health               - Sistem sağlık durumu
  monitor              - Gerçek zamanlı sistem izleme
  benchmark            - Performans testi çalıştır
  wait-for-ready       - Servis hazır olana kadar bekle

Seçenekler:
  --grpc-endpoint <addr> - GRPC endpoint (varsayılan: llm-llama-service:16071)
  --http-endpoint <addr> - HTTP endpoint (varsayılan: llm-llama-service:16070)
  --timeout <seconds>    - İstekler için zaman aşımı süresi (saniye)
  --iterations <n>       - Benchmark iterasyon sayısı
  --output <file>        - Benchmark raporu için çıktı dosyası

Örnekler:
  llm_cli generate "Türkiye'nin başkenti neresidir?"
  llm_cli --http-endpoint localhost:16070 health
  llm_cli benchmark --iterations 50
)";
}

int main(int argc, char** argv) {
    spdlog::set_level(spdlog::level::info);

    if (argc < 2) {
        print_usage();
        return 1;
    }

    std::vector<std::string> args(argv + 1, argv + argc);
    std::map<std::string, std::string> options;
    std::string command;
    std::vector<std::string> command_args;

    // Argümanları ayrıştır: önce seçenekleri, sonra komutu ve argümanlarını bul
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i].rfind("--", 0) == 0) {
            std::string key = args[i].substr(2);
            if (i + 1 < args.size() && args[i + 1].rfind("--", 0) != 0) {
                options[key] = args[++i];
            } else {
                options[key] = "true";
            }
        } else if (command.empty()) {
            command = args[i];
        } else {
            command_args.push_back(args[i]);
        }
    }

    if (command.empty()) {
        print_usage();
        return 1;
    }
    
    // Varsayılan endpoint'i Docker ağındaki servis adına göre ayarla
    std::string grpc_endpoint = options.count("grpc-endpoint") ? 
                               options["grpc-endpoint"] : "llm-llama-service:16071";
    std::string http_endpoint = options.count("http-endpoint") ? 
                               options["http-endpoint"] : "llm-llama-service:16070";
    
    try {
        if (command == "generate") {
            if (command_args.empty()) {
                spdlog::error("generate komutu için bir prompt metni gereklidir.");
                print_usage();
                return 1;
            }
            std::string prompt;
            for (const auto& arg : command_args) {
                prompt += arg + " ";
            }
            prompt.pop_back();

            sentiric_llm_cli::CLIClient client(grpc_endpoint, http_endpoint);
            
            if (options.count("timeout")) {
                client.set_timeout(std::stoi(options["timeout"]));
            }
            
            std::cout << "👤 Kullanıcı: " << prompt << "\n";
            
            bool success = client.generate_stream(prompt, 
                [](const std::string& token) {
                    std::cout << token << std::flush;
                }
            );
            std::cout << std::endl;
            
            if (!success) {
                spdlog::error("Generation başarısız.");
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
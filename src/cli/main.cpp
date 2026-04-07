#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "benchmark.h"
#include "grpc_client.h"
#include "health_check.h"
#include "nlohmann/json.hpp"
#include "spdlog/spdlog.h"

void print_usage() {
  std::cout << R"(
🧠 Sentiric LLM CLI v2.7.2

Kullanım:
  llm_cli [seçenekler] <komut> [argümanlar]

Komutlar:
  generate <prompt>        - Metin üretir.
  health                   - Sistem sağlık durumunu kontrol eder.
  wait-for-ready           - Servis hazır olana kadar bekler.
  benchmark                - Performans testi çalıştırır.
  interrupt-test           - Voice Gateway söz kesme senaryosunu simüle eder.

Seçenekler:
  --grpc-endpoint <addr>   - GRPC endpoint (varsayılan: llm-llama-service:16071).
  --http-endpoint <addr>   - HTTP endpoint (varsayılan: llm-llama-service:16070).
  --iterations <n>         - Tekli test iterasyon sayısı.
  --concurrent <n>         - Eşzamanlı bağlantı sayısı.
  --requests <n>           - Bağlantı başına istek sayısı.
  --output <file>          - Raporu dosyaya kaydeder (opsiyonel).
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

  std::string grpc_endpoint = options.count("grpc-endpoint")
                                  ? options["grpc-endpoint"]
                                  : "llm-llama-service:16071";
  std::string http_endpoint = options.count("http-endpoint")
                                  ? options["http-endpoint"]
                                  : "llm-llama-service:16070";

  try {
    if (command == "generate") {
      if (command_args.empty()) {
        spdlog::error("generate komutu için girdi gereklidir.");
        return 1;
      }
      std::string prompt;
      for (const auto& arg : command_args) {
        prompt += arg + " ";
      }
      prompt.pop_back();
      sentiric::llm::v1::GenerateStreamRequest request;
      request.set_user_prompt(prompt);
      sentiric_llm_cli::GRPCClient client(grpc_endpoint);
      std::cout << "🤖 Assistant: " << std::flush;
      client.generate_stream(request, [](const std::string& token) {
        std::cout << token << std::flush;
      });
      std::cout << std::endl;
    } else if (command == "health" || command == "wait-for-ready") {
      sentiric_llm_cli::HealthChecker checker(grpc_endpoint, http_endpoint);
      if (command == "health") {
        checker.print_detailed_status();
      } else {
        if (!checker.wait_for_ready()) return 1;
      }
    } else if (command == "benchmark") {
      int iter =
          options.count("iterations") ? std::stoi(options["iterations"]) : 10;
      int concurrent =
          options.count("concurrent") ? std::stoi(options["concurrent"]) : 1;
      int reqs = options.count("requests") ? std::stoi(options["requests"]) : 1;
      std::string outfile = options.count("output") ? options["output"] : "";

      sentiric_llm_cli::Benchmark benchmark(grpc_endpoint);
      sentiric_llm_cli::BenchmarkResult result;

      if (concurrent > 1) {
        // [FIX] Eşzamanlı test sonucunu yakala
        result = benchmark.run_concurrent_test(concurrent, reqs);
      } else {
        // [FIX] Tekli test sonucunu yakala
        result = benchmark.run_performance_test(iter);
      }

      // [FIX] Raporu mutlaka üret
      benchmark.generate_report(result, outfile);

    } else if (command == "interrupt-test") {
      sentiric_llm_cli::Benchmark benchmark(grpc_endpoint);
      std::string initial =
          "Merhaba, siparişim ne alemde? Kargoya verildi mi, kargo takip "
          "numarasını ve teslimat adresini öğrenebilir miyim?";
      std::string interrupt =
          "Pardon sözünü kestim, sadece kargo numarasını alabilir miyim?";
      benchmark.run_interrupt_test(initial, interrupt);
    } else {
      spdlog::error("Geçersiz komut: '{}'", command);
      print_usage();
      return 1;
    }
  } catch (const std::exception& e) {
    spdlog::critical("CLI hatası: {}", e.what());
    return 1;
  }
  return 0;
}
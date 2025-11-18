// src/cli/main.cpp - YENİ İÇERİK
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include "spdlog/spdlog.h"
#include "grpc_client.h"
#include "health_check.h"
#include "benchmark.h"
#include "nlohmann/json.hpp" // JSON parsing için eklendi

void print_usage() {
    std::cout << R"(
🧠 Sentiric LLM CLI v2.1

Kullanım:
  llm_cli [seçenekler] <komut> [argümanlar]

Komutlar:
  generate <user_prompt>   - Zengin bir bağlam ile metin üretir (GRPC stream).
  health                   - Sistem sağlık durumunu kontrol eder.
  wait-for-ready           - Servis hazır olana kadar bekler.
  benchmark                - Performans testi çalıştırır.

Seçenekler:
  --system-prompt <text>   - (Opsiyonel) AI'nın kişiliğini belirleyen sistem talimatı.
  --rag-context <text>     - (Opsiyonel) RAG için kullanılacak bilgi metni.
  --history '<json_string>'- (Opsiyonel) Konuşma geçmişi. Örn: '[{"role":"user","content":"Merhaba"},{"role":"assistant","content":"Merhaba, nasıl yardımcı olabilirim?"}]'
  --grpc-endpoint <addr>   - GRPC endpoint (varsayılan: llm-llama-service:16071).
  --http-endpoint <addr>   - HTTP endpoint (varsayılan: llm-llama-service:16070).
  --timeout <seconds>      - İstek zaman aşımı süresi (saniye, varsayılan: 120).
  --iterations <n>         - Benchmark iterasyon sayısı.
  --output <file>          - Benchmark raporu için çıktı dosyası.

Örnekler:
  # Basit Soru
  llm_cli generate "Türkiye'nin başkenti neresidir?"

  # Konuşma Geçmişi ile Takip Sorusu
  llm_cli generate "Peki yüzölçümü ne kadar?" --history '[{"role":"user","content":"Türkiyenin başkenti neresidir?"},{"role":"assistant","content":"Türkiyenin başkenti Ankaradır."}]'
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
            } else { options[key] = "true"; }
        } else if (command.empty()) {
            command = args[i];
        } else {
            command_args.push_back(args[i]);
        }
    }
    
    std::string grpc_endpoint = options.count("grpc-endpoint") ? options["grpc-endpoint"] : "llm-llama-service:16071";
    std::string http_endpoint = options.count("http-endpoint") ? options["http-endpoint"] : "llm-llama-service:16070";

    try {
        if (command == "generate") {
            if (command_args.empty()) { 
                spdlog::error("generate komutu için bir kullanıcı girdisi gereklidir.");
                print_usage();
                return 1;
            }
            std::string user_prompt;
            for (const auto& arg : command_args) { user_prompt += arg + " "; }
            user_prompt.pop_back();

            sentiric::llm::v1::LLMLocalServiceGenerateStreamRequest request;
            request.set_user_prompt(user_prompt);

            if (options.count("system-prompt")) {
                request.set_system_prompt(options["system-prompt"]);
            }
            if (options.count("rag-context")) {
                request.set_rag_context(options["rag-context"]);
            }

            // --- YENİ: Konuşma geçmişini parse et ---
            if (options.count("history")) {
                try {
                    auto history_json = nlohmann::json::parse(options["history"]);
                    if (history_json.is_array()) {
                        for (const auto& item : history_json) {
                            if (item.is_object() && item.contains("role") && item.contains("content")) {
                                auto* turn = request.add_history();
                                turn->set_role(item["role"]);
                                turn->set_content(item["content"]);
                            }
                        }
                    }
                } catch (const nlohmann::json::parse_error& e) {
                    spdlog::error("--history argümanı geçerli bir JSON değil: {}", e.what());
                    return 1;
                }
            }
            
            sentiric_llm_cli::GRPCClient client(grpc_endpoint);
            if (options.count("timeout")) { client.set_timeout(std::stoi(options["timeout"])); }

            std::cout << "🤖 Assistant: " << std::flush;
            
            bool success = client.generate_stream(request, [](const std::string& token) {
                std::cout << token << std::flush;
            });
            std::cout << std::endl;

            if (!success) {
                spdlog::error("Generation başarısız oldu.");
                return 1;
            }

        } else if (command == "health" || command == "wait-for-ready") {
             sentiric_llm_cli::HealthChecker checker(grpc_endpoint, http_endpoint);
             if (command == "health") {
                 checker.print_detailed_status();
             } else {
                 int timeout = options.count("timeout") ? std::stoi(options["timeout"]) : 300;
                 if (!checker.wait_for_ready(timeout)) return 1;
             }
        } else if (command == "benchmark") {
             int iterations = options.count("iterations") ? std::stoi(options["iterations"]) : 10;
             std::string output_file = options.count("output") ? options["output"] : "";
             sentiric_llm_cli::Benchmark benchmark(grpc_endpoint);
             auto result = benchmark.run_performance_test(iterations);
             benchmark.generate_report(result, output_file);
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
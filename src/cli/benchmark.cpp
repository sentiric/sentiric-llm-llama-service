#include "benchmark.h"

#include <atomic>
#include <fstream>
#include <future>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

#include "grpc_client.h"
#include "spdlog/spdlog.h"

namespace sentiric_llm_cli {

Benchmark::Benchmark(const std::string& grpc_endpoint)
    : grpc_endpoint_(grpc_endpoint),
      client_(std::make_unique<CLIClient>(grpc_endpoint)) {}

BenchmarkResult Benchmark::run_performance_test(int iterations,
                                                const std::string& prompt) {
  spdlog::info("🎯 Performans testi başlıyor ({} iterasyon)...", iterations);
  BenchmarkResult result;
  result.total_requests = iterations;
  long long total_tokens_generated = 0;
  auto total_start = std::chrono::steady_clock::now();
  double total_response_time_ms = 0;
  for (int i = 0; i < iterations; i++) {
    auto start_time = std::chrono::steady_clock::now();
    std::atomic<int> current_run_tokens = 0;
    bool success = client_->generate_stream(
        prompt, [&](const std::string& /*token*/) { current_run_tokens++; });
    auto end_time = std::chrono::steady_clock::now();
    double response_time =
        std::chrono::duration_cast<std::chrono::milliseconds>(end_time -
                                                              start_time)
            .count();
    result.response_times.push_back(response_time);
    total_response_time_ms += response_time;
    total_tokens_generated += current_run_tokens.load();
    if (success) {
      result.successful_requests++;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  auto total_end = std::chrono::steady_clock::now();
  result.total_duration =
      std::chrono::duration_cast<std::chrono::seconds>(total_end - total_start);
  result.average_response_time_ms =
      iterations > 0 ? total_response_time_ms / iterations : 0;
  double duration_sec =
      std::chrono::duration<double>(total_end - total_start).count();
  if (duration_sec > 0) {
    result.tokens_per_second = total_tokens_generated / duration_sec;
  } else {
    result.tokens_per_second = 0;
  }
  result.error_rate =
      iterations > 0
          ? ((iterations - result.successful_requests) * 100.0) / iterations
          : 0;
  result.total_tokens_generated = total_tokens_generated;
  spdlog::info("✅ Performans testi tamamlandı");
  return result;
}

BenchmarkResult Benchmark::run_concurrent_test(int concurrent_connections,
                                               int requests_per_connection) {
  spdlog::info("⚡ Eşzamanlı test başlıyor ({} bağlantı, {} istek/bağlantı)...",
               concurrent_connections, requests_per_connection);
  BenchmarkResult result;
  result.total_requests = concurrent_connections * requests_per_connection;
  std::atomic<int> success_count{0};
  std::atomic<long long> total_tokens{0};
  std::atomic<double> total_latency_sum{0.0};
  auto total_start = std::chrono::steady_clock::now();
  std::vector<std::future<void>> futures;
  for (int i = 0; i < concurrent_connections; i++) {
    futures.push_back(std::async(
        std::launch::async, [this, requests_per_connection, &success_count,
                             &total_tokens, &total_latency_sum]() {
          auto client = std::make_unique<CLIClient>(this->grpc_endpoint_);
          for (int j = 0; j < requests_per_connection; j++) {
            auto req_start = std::chrono::steady_clock::now();
            int tokens_in_req = 0;
            bool success = client->generate_stream(
                "Concurrent test prompt " + std::to_string(j),
                [&tokens_in_req](const std::string&) { tokens_in_req++; });
            auto req_end = std::chrono::steady_clock::now();
            double latency =
                std::chrono::duration_cast<std::chrono::milliseconds>(req_end -
                                                                      req_start)
                    .count();
            if (success) {
              success_count++;
              total_tokens += tokens_in_req;
              total_latency_sum = total_latency_sum.load() + latency;
            }
          }
        }));
  }
  for (auto& future : futures) future.wait();
  auto total_end = std::chrono::steady_clock::now();
  result.total_duration =
      std::chrono::duration_cast<std::chrono::seconds>(total_end - total_start);
  double duration_sec =
      std::chrono::duration<double>(total_end - total_start).count();
  result.successful_requests = success_count.load();
  result.total_tokens_generated = total_tokens.load();
  result.error_rate =
      result.total_requests > 0
          ? ((result.total_requests - result.successful_requests) * 100.0) /
                result.total_requests
          : 0;
  if (result.successful_requests > 0) {
    result.average_response_time_ms =
        total_latency_sum.load() / result.successful_requests;
  } else {
    result.average_response_time_ms = 0;
  }
  if (duration_sec > 0) {
    result.tokens_per_second = result.total_tokens_generated / duration_sec;
  } else {
    result.tokens_per_second = 0;
  }
  spdlog::info("✅ Eşzamanlı test tamamlandı");
  return result;
}

void Benchmark::run_interrupt_test(const std::string& initial_prompt,
                                   const std::string& interrupt_prompt) {
  spdlog::info("🎤 Voice Gateway Interruption Test Başlatılıyor...");

  auto client1 = std::make_unique<sentiric_llm_cli::GRPCClient>(grpc_endpoint_);
  auto client2 = std::make_unique<sentiric_llm_cli::GRPCClient>(grpc_endpoint_);

  std::string partial_response;
  std::mutex response_mutex;

  sentiric::llm::v1::GenerateStreamRequest req1;
  req1.set_user_prompt(initial_prompt);
  req1.set_rag_context(
      "Sipariş No: #ABC-123. Durum: Kargoya verildi. Kargo Takip No: "
      "TRK-987654321.");

  std::cout << "\n\033[1;34m🙍‍♂️ KULLANICI:\033[0m " << initial_prompt
            << std::endl;
  spdlog::info("⚡ GATEWAY: LLM'e istek gönderiliyor...");
  std::cout << "\033[1;32m🤖 ASİSTAN:\033[0m " << std::flush;

  auto llm_future = std::async(std::launch::async, [&]() {
    client1->generate_stream_with_cancellation(
        req1, [&](const std::string& token) {
          std::lock_guard<std::mutex> lock(response_mutex);
          std::cout << token << std::flush;
          partial_response += token;
        });
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(1500));
  spdlog::info(
      "\n⚡ GATEWAY: !!! KULLANICI KONUŞMAYA BAŞLADI - STREAM KESİLİYOR !!!");
  client1->cancel_stream();
  llm_future.wait();

  {
    std::lock_guard<std::mutex> lock(response_mutex);
    spdlog::info("⚡ GATEWAY: AI'ın yarım cümlesi: \"" + partial_response +
                 "...\"");
  }

  sentiric::llm::v1::GenerateStreamRequest req2;
  req2.set_user_prompt(interrupt_prompt);
  // [DÜZELTME] String literali doğru kapatıldı.
  req2.set_rag_context(
      "Sipariş No: #ABC-123. Durum: Kargoya verildi. Kargo Takip No: "
      "TRK-987654321.");

  auto* turn1 = req2.add_history();
  turn1->set_role("user");
  turn1->set_content(initial_prompt);

  auto* turn2 = req2.add_history();
  turn2->set_role("assistant");
  turn2->set_content(partial_response);

  std::cout << "\n\033[1;34m🙍‍♂️ KULLANICI:\033[0m "
            << interrupt_prompt << std::endl;
  spdlog::info("⚡ GATEWAY: LLM'e yeni istek gönderiliyor...");
  std::cout << "\033[1;32m🤖 ASİSTAN:\033[0m " << std::flush;

  client2->generate_stream(
      req2, [](const std::string& token) { std::cout << token << std::flush; });
  std::cout << std::endl;
  spdlog::info("✅ Simülasyon Tamamlandı.");
}

void Benchmark::generate_report(const BenchmarkResult& result,
                                const std::string& filename) {
  std::ostream* output = &std::cout;
  std::ofstream file;
  if (!filename.empty()) {
    file.open(filename);
    output = &file;
  }
  *output << "📊 PERFORMANS TEST RAPORU\n";
  *output << "========================\n";
  *output << "Toplam İstek: " << result.total_requests << "\n";
  *output << "Başarılı İstek: " << result.successful_requests << "\n";
  *output << "Toplam Üretilen Token: " << result.total_tokens_generated << "\n";
  *output << "Hata Oranı: " << std::fixed << std::setprecision(2)
          << result.error_rate << "%\n";
  *output << "Ortalama Yanıt Süresi: " << result.average_response_time_ms
          << " ms\n";
  *output << "Token/Saniye (TPS): " << std::fixed << std::setprecision(2)
          << result.tokens_per_second << "\n";
  *output << "Toplam Süre: " << result.total_duration.count() << " saniye\n";
  *output << "========================\n";
  if (!result.response_times.empty()) {
    *output << "\nYanıt Süreleri (ms):\n";
    for (size_t i = 0; i < result.response_times.size(); i++)
      *output << "  " << (i + 1) << ": " << result.response_times[i] << " ms\n";
  }
  if (file.is_open()) {
    file.close();
    spdlog::info("Rapor dosyaya kaydedildi: {}", filename);
  }
}

}  // namespace sentiric_llm_cli
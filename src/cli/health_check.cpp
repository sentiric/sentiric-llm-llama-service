#include "health_check.h"

#include <chrono>
#include <iostream>  // BU SATIRI EKLE
#include <thread>

#include "spdlog/spdlog.h"

namespace sentiric_llm_cli {

HealthChecker::HealthChecker(const std::string& grpc_endpoint,
                             const std::string& http_endpoint)
    : client_(std::make_unique<CLIClient>(grpc_endpoint, http_endpoint)) {}

HealthChecker::SystemStatus HealthChecker::check_system() {
  SystemStatus status;
  auto start_time = std::chrono::steady_clock::now();

  // HTTP health check
  auto http_status = client_->get_health_status();
  status.http_healthy = (http_status == "healthy");

  // GRPC health check (basit bir test)
  status.grpc_healthy = client_->is_connected();

  // Model readiness
  status.model_ready = client_->health_check();

  auto end_time = std::chrono::steady_clock::now();
  status.response_time_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(end_time -
                                                            start_time)
          .count();

  // Overall status
  if (status.http_healthy && status.grpc_healthy && status.model_ready) {
    status.overall_status = "HEALTHY";
  } else if (status.http_healthy || status.grpc_healthy) {
    status.overall_status = "DEGRADED";
  } else {
    status.overall_status = "UNHEALTHY";
  }

  return status;
}

bool HealthChecker::wait_for_ready(int max_wait_seconds, int check_interval) {
  spdlog::info("Servis hazır olana kadar bekleniyor (max {} saniye)...",
               max_wait_seconds);

  auto start_time = std::chrono::steady_clock::now();
  int attempts = 0;

  while (true) {
    attempts++;
    auto status = check_system();

    if (status.overall_status == "HEALTHY") {
      spdlog::info("✅ Servis hazır! ({} deneme sonunda)", attempts);
      return true;
    }

    auto current_time = std::chrono::steady_clock::now();
    auto elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>(
                               current_time - start_time)
                               .count();

    if (elapsed_seconds >= max_wait_seconds) {
      spdlog::error("❌ Servis hazır olma zaman aşımı ({} saniye)",
                    max_wait_seconds);
      return false;
    }

    spdlog::info("⏳ Deneme {} - Durum: {} ({} saniye kaldı)", attempts,
                 status.overall_status, max_wait_seconds - elapsed_seconds);

    std::this_thread::sleep_for(std::chrono::seconds(check_interval));
  }
}

void HealthChecker::print_detailed_status() {
  auto status = check_system();

  std::cout << "\n📊 SİSTEM DURUM RAPORU\n";
  std::cout << "====================\n";
  std::cout << "🔧 Genel Durum: " << status.overall_status << "\n";
  std::cout << "🌐 HTTP Sağlık: "
            << (status.http_healthy ? "✅ SAĞLIKLI" : "❌ SAĞLIKSIZ") << "\n";
  std::cout << "🔌 GRPC Bağlantı: "
            << (status.grpc_healthy ? "✅ BAĞLI" : "❌ BAĞLANTISIZ") << "\n";
  std::cout << "🧠 Model Hazır: "
            << (status.model_ready ? "✅ HAZIR" : "❌ HAZIR DEĞİL") << "\n";
  std::cout << "⏱️  Yanıt Süresi: " << status.response_time_ms << " ms\n";
  std::cout << "====================\n\n";
}

}  // namespace sentiric_llm_cli
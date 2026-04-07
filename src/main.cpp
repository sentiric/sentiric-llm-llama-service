// Dosya: src/main.cpp
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
#include <grpcpp/security/server_credentials.h>
#include <prometheus/counter.h>
#include <prometheus/gauge.h>
#include <prometheus/histogram.h>
#include <prometheus/registry.h>
#include <spdlog/sinks/stdout_sinks.h>  // Renksiz (JSON uyumlu)
#include <spdlog/spdlog.h>

#include <csignal>
#include <fstream>
#include <future>
#include <memory>
#include <sstream>
#include <thread>

#include "config.h"
#include "core/model_warmup.h"
#include "grpc_server.h"
#include "http_server.h"
#include "llama.h"
#include "llm_engine.h"
#include "suts_logger.h"  // [YENİ]

namespace {
std::promise<void> shutdown_promise;
}

void llama_log_callback(ggml_log_level level, const char* text,
                        void* user_data) {
  (void)user_data;
  std::string_view text_view(text);
  if (!text_view.empty() && text_view.back() == '\n') {
    text_view.remove_suffix(1);
  }

  if (text_view == "." ||
      text_view.find("llama_model_loader:") != std::string::npos)
    return;

  switch (level) {
    case GGML_LOG_LEVEL_ERROR:
      SUTS_ERROR("LLAMA_CPP_ERROR", "", "", "", "[llama.cpp] {}", text_view);
      break;
    case GGML_LOG_LEVEL_WARN:
      SUTS_WARN("LLAMA_CPP_WARN", "", "", "", "[llama.cpp] {}", text_view);
      break;
    case GGML_LOG_LEVEL_INFO:
      SUTS_DEBUG("LLAMA_CPP_INFO", "", "", "", "[llama.cpp] {}", text_view);
      break;
    case GGML_LOG_LEVEL_DEBUG:
      SUTS_DEBUG("LLAMA_CPP_DEBUG", "", "", "", "[llama.cpp] {}", text_view);
      break;
    default:
      break;
  }
}

void signal_handler(int signal) {
  SUTS_WARN("SERVICE_SHUTDOWN", "", "", "",
            "🛑 Signal {} received. Initiating graceful shutdown...", signal);
  try {
    shutdown_promise.set_value();
  } catch (const std::future_error&) {
  }
}

std::string read_file(const std::string& filepath) {
  std::ifstream file(filepath);
  if (!file.is_open()) {
    throw std::runtime_error("File could not be opened: " + filepath);
  }
  std::stringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

void setup_logging() {
  const char* log_level_env = std::getenv("LLM_LLAMA_SERVICE_LOG_LEVEL");
  std::string level = log_level_env ? log_level_env : "info";

  //[ARCH-COMPLIANCE] Strict SUTS v4.0 Formatter Kullanımı (Renksiz JSON)
  auto sink = std::make_shared<spdlog::sinks::stdout_sink_mt>();
  sink->set_formatter(std::make_unique<suts::SutsFormatter>());

  auto logger = std::make_shared<spdlog::logger>("llm-llama-service", sink);
  spdlog::set_default_logger(logger);
  spdlog::set_level(spdlog::level::from_str(level));
}

int main() {
  setup_logging();

  llama_log_set(llama_log_callback, nullptr);

  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);

  SUTS_INFO("SERVICE_START", "", "", "",
            "🚀 Sentiric LLM Llama Service starting...v3.2.1");

  auto settings = load_settings();

  SUTS_INFO(
      "SERVICE_CONFIGURED", "", "", "",
      "ℹ️ Final Configuration: Profile='{}', Ctx={}, GPU Layers={}, Batch={}",
      settings.profile_name, settings.context_size, settings.n_gpu_layers,
      settings.max_batch_size);

  auto registry = std::make_shared<prometheus::Registry>();

  auto& requests_total_family = prometheus::BuildCounter()
                                    .Name("llm_requests_total")
                                    .Help("Total number of gRPC requests")
                                    .Register(*registry);

  auto& request_latency_family = prometheus::BuildHistogram()
                                     .Name("llm_request_latency_seconds")
                                     .Help("Histogram of gRPC request latency")
                                     .Register(*registry);

  auto& tokens_generated_total_family =
      prometheus::BuildCounter()
          .Name("llm_tokens_generated_total")
          .Help("Total number of tokens generated")
          .Register(*registry);

  auto& active_contexts_family =
      prometheus::BuildGauge()
          .Name("llm_active_contexts")
          .Help("Current number of active llama_context instances")
          .Register(*registry);

  AppMetrics metrics = {
      requests_total_family.Add({}),
      request_latency_family.Add(
          {}, prometheus::Histogram::BucketBoundaries{0.001, 0.005, 0.01, 0.05,
                                                      0.1, 0.5, 1.0, 5.0}),
      tokens_generated_total_family.Add({}), active_contexts_family.Add({})};

  std::unique_ptr<grpc::Server> grpc_server_ptr;
  std::shared_ptr<HttpServer> http_server;
  std::shared_ptr<MetricsServer> metrics_server;
  std::thread http_thread;
  std::thread grpc_thread;
  std::thread metrics_thread;

  try {
    grpc::EnableDefaultHealthCheckService(true);

    auto engine =
        std::make_shared<LLMEngine>(settings, metrics.active_contexts);

    if (!engine->is_model_loaded()) {
      SUTS_ERROR("MODEL_LOAD_FAIL", "", "", "",
                 "🔥 LLM Engine failed to initialize with a valid model. "
                 "Shutting down.");
      return 1;
    }

    if (settings.enable_warm_up) {
      SUTS_INFO("MODEL_WARMUP_START", "", "", "",
                "🌡️ Starting model warm-up...");
      ModelWarmup::fast_warmup(engine->get_context_pool(), settings.n_threads);
      SUTS_INFO("MODEL_WARMUP_END", "", "", "", "✅ Model warm-up completed");
    }

    std::string grpc_address =
        settings.host + ":" + std::to_string(settings.grpc_port);
    GrpcServer grpc_service(engine, metrics);
    grpc::ServerBuilder builder;

    if (settings.grpc_ca_path.empty() || settings.grpc_cert_path.empty() ||
        settings.grpc_key_path.empty()) {
      const char* env_p = std::getenv("ENV");
      if (env_p && std::string(env_p) == "production") {
        // [ARCH-COMPLIANCE] constraints.yaml kural ihlali engellemesi
        // (security.grpc_communication)
        throw std::runtime_error(
            "Strict Compliance Error: mTLS credentials are MANDATORY in "
            "production environment for gRPC.");
      }
      SUTS_WARN("TLS_INSECURE_MODE", "", "", "",
                "⚠️ gRPC TLS path variables not fully set. Using insecure "
                "credentials.");
      builder.AddListeningPort(grpc_address, grpc::InsecureServerCredentials());
    } else {
      SUTS_INFO("TLS_SECURE_MODE", "", "", "",
                "🔐 Loading TLS credentials for gRPC server...");
      std::string root_ca = read_file(settings.grpc_ca_path);
      std::string server_cert = read_file(settings.grpc_cert_path);
      std::string server_key = read_file(settings.grpc_key_path);

      grpc::SslServerCredentialsOptions::PemKeyCertPair pkcp = {server_key,
                                                                server_cert};
      grpc::SslServerCredentialsOptions ssl_opts;
      ssl_opts.pem_root_certs = root_ca;
      ssl_opts.pem_key_cert_pairs.push_back(pkcp);
      ssl_opts.client_certificate_request =
          GRPC_SSL_REQUEST_AND_REQUIRE_CLIENT_CERTIFICATE_AND_VERIFY;

      auto creds = grpc::SslServerCredentials(ssl_opts);
      builder.AddListeningPort(grpc_address, creds);
      SUTS_INFO("TLS_CONFIGURED", "", "", "",
                "✅ gRPC server configured to use mTLS.");
    }

    builder.RegisterService(&grpc_service);
    grpc_server_ptr = builder.BuildAndStart();

    if (!grpc_server_ptr) {
      SUTS_ERROR("GRPC_BIND_FAIL", "", "", "",
                 "🔥 gRPC server failed to start on {}", grpc_address);
      return 1;
    }

    auto health_service = grpc_server_ptr->GetHealthCheckService();
    if (health_service) {
      health_service->SetServingStatus("sentiric.llm.v1.LlamaService", true);
      health_service->SetServingStatus("", true);
    }

    SUTS_INFO("GRPC_SERVER_READY", "", "", "", "📡 gRPC server listening on {}",
              grpc_address);

    http_server = std::make_shared<HttpServer>(
        engine, settings.host, settings.http_port, settings.http_threads);
    metrics_server = std::make_shared<MetricsServer>(
        settings.host, settings.metrics_port, *registry);

    grpc_thread = std::thread(&grpc::Server::Wait, grpc_server_ptr.get());
    http_thread = std::thread(&HttpServer::run, http_server);
    metrics_thread = std::thread(&MetricsServer::run, metrics_server);

    SUTS_INFO("ALL_SERVERS_READY", "", "", "",
              "✅ All servers started successfully");

    auto shutdown_future = shutdown_promise.get_future();
    shutdown_future.wait();

    if (health_service) health_service->SetServingStatus("", false);
    http_server->stop();
    metrics_server->stop();
    grpc_server_ptr->Shutdown(std::chrono::system_clock::now() +
                              std::chrono::seconds(5));

    if (http_thread.joinable()) http_thread.join();
    if (metrics_thread.joinable()) metrics_thread.join();
    if (grpc_thread.joinable()) grpc_thread.join();

  } catch (const std::exception& e) {
    SUTS_ERROR("SERVICE_CRASH", "", "", "", "🔥 Fatal error: {}", e.what());
    if (http_server) http_server->stop();
    if (metrics_server) metrics_server->stop();
    if (grpc_server_ptr) grpc_server_ptr->Shutdown();
    return 1;
  }

  return 0;
}
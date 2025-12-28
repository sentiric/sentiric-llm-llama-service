#include "config.h"
#include "llm_engine.h"
#include "grpc_server.h"
#include "http_server.h"
#include "core/model_warmup.h"
#include <thread>
#include <memory>
#include <csignal>
#include <future>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h> // GEREKLİ: Health Check Interface
#include <grpcpp/security/server_credentials.h>
#include <fstream>
#include <sstream>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include "llama.h"

#include <prometheus/registry.h>
#include <prometheus/counter.h>
#include <prometheus/histogram.h>
#include <prometheus/gauge.h>

namespace {
    std::promise<void> shutdown_promise;
}

// Log Filtreleme: Gereksiz Llama loglarını gizle
void llama_log_callback(ggml_log_level level, const char * text, void * user_data) {
    (void)user_data;
    std::string_view text_view(text);
    if (!text_view.empty() && text_view.back() == '\n') {
        text_view.remove_suffix(1);
    }
    
    // Filtreleme: "." gibi progress logları veya çok düşük seviye detaylar
    if (text_view == "." || text_view.find("llama_model_loader:") != std::string::npos) return;

    switch (level) {
        case GGML_LOG_LEVEL_ERROR: spdlog::error("[llama.cpp] {}", text_view); break;
        case GGML_LOG_LEVEL_WARN: spdlog::warn("[llama.cpp] {}", text_view); break;
        case GGML_LOG_LEVEL_INFO: spdlog::trace("[llama.cpp] {}", text_view); break; // INFO -> TRACE (Gizle)
        case GGML_LOG_LEVEL_DEBUG: spdlog::trace("[llama.cpp] {}", text_view); break;
        default: break;
    }
}

void signal_handler(int signal) {
    spdlog::warn("🛑 Signal {} received. Initiating graceful shutdown...", signal);
    try {
        shutdown_promise.set_value();
    } catch (const std::future_error&) {}
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
    const char* env_p = std::getenv("ENV");
    std::string env = env_p ? std::string(env_p) : "development";
    auto sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    
    if (env == "production") {
        // JSON Log formatı (Microservices uyumlu)
        sink->set_pattern(R"({"timestamp":"%Y-%m-%dT%T.%fZ","level":"%l","service":"llm-llama-service","message":"%v"})");
    } else {
        // İnsan okunabilir format
        sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
    }
    
    auto logger = std::make_shared<spdlog::logger>("llm-llama-service", sink);
    spdlog::set_default_logger(logger);
}

int main() {
    setup_logging();
    
    // Log level'ı en başta ayarla ki config logları görünsün
    const char* log_level_env = std::getenv("LLM_LLAMA_SERVICE_LOG_LEVEL");
    std::string level = log_level_env ? log_level_env : "info";
    spdlog::set_level(spdlog::level::from_str(level));

    llama_log_set(llama_log_callback, nullptr);

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    spdlog::info("🚀 Sentiric LLM Llama Service starting...");

    // AYARLARI YÜKLE (LOGLAR BURADA BASILACAK)
    auto settings = load_settings();
    
    // Eğer config'den gelen log level farklıysa güncelle
    spdlog::set_level(spdlog::level::from_str(settings.log_level));
    spdlog::info("ℹ️ Final Configuration: Profile='{}', Ctx={}, GPU Layers={}, Batch={}", 
                 settings.profile_name, settings.context_size, settings.n_gpu_layers, settings.max_batch_size);

    // 1. Prometheus Metrik Altyapısını Kur
    auto registry = std::make_shared<prometheus::Registry>();

    auto& requests_total_family = prometheus::BuildCounter()
        .Name("llm_requests_total")
        .Help("Total number of gRPC requests")
        .Register(*registry);

    auto& request_latency_family = prometheus::BuildHistogram()
        .Name("llm_request_latency_seconds")
        .Help("Histogram of gRPC request latency")
        .Register(*registry);
    
    auto& tokens_generated_total_family = prometheus::BuildCounter()
        .Name("llm_tokens_generated_total")
        .Help("Total number of tokens generated")
        .Register(*registry);

    auto& active_contexts_family = prometheus::BuildGauge()
        .Name("llm_active_contexts")
        .Help("Current number of active llama_context instances")
        .Register(*registry);

    AppMetrics metrics = {
        requests_total_family.Add({}),
        request_latency_family.Add({}, prometheus::Histogram::BucketBoundaries{0.001, 0.005, 0.01, 0.05, 0.1, 0.5, 1.0, 5.0}),
        tokens_generated_total_family.Add({}),
        active_contexts_family.Add({})
    };

    std::unique_ptr<grpc::Server> grpc_server_ptr;
    std::shared_ptr<HttpServer> http_server;
    std::shared_ptr<MetricsServer> metrics_server;
    std::thread http_thread;
    std::thread grpc_thread;
    std::thread metrics_thread;

    try {
        // --- ENABLE STANDARD GRPC HEALTH CHECK ---
        grpc::EnableDefaultHealthCheckService(true);

        auto engine = std::make_shared<LLMEngine>(settings, metrics.active_contexts);
        
        if (!engine->is_model_loaded()) {
            spdlog::critical("🔥 LLM Engine failed to initialize with a valid model. Shutting down.");
            return 1;
        }

        // --- MODEL WARM-UP ---
        if (settings.enable_warm_up) {
            spdlog::info("🌡️ Starting model warm-up...");
            ModelWarmup::fast_warmup(engine->get_context_pool(), settings.n_threads);
            spdlog::info("✅ Model warm-up completed");
        }

        std::string grpc_address = settings.host + ":" + std::to_string(settings.grpc_port);
        GrpcServer grpc_service(engine, metrics);
        grpc::ServerBuilder builder;

        if (settings.grpc_ca_path.empty() || settings.grpc_cert_path.empty() || settings.grpc_key_path.empty()) {
            spdlog::warn("⚠️ gRPC TLS path variables not fully set. Using insecure credentials.");
            builder.AddListeningPort(grpc_address, grpc::InsecureServerCredentials());
        } else {
            spdlog::info("🔐 Loading TLS credentials for gRPC server...");
            std::string root_ca = read_file(settings.grpc_ca_path);
            std::string server_cert = read_file(settings.grpc_cert_path);
            std::string server_key = read_file(settings.grpc_key_path);

            grpc::SslServerCredentialsOptions::PemKeyCertPair pkcp = {server_key, server_cert};
            grpc::SslServerCredentialsOptions ssl_opts;
            ssl_opts.pem_root_certs = root_ca;
            ssl_opts.pem_key_cert_pairs.push_back(pkcp);
            ssl_opts.client_certificate_request = GRPC_SSL_REQUEST_AND_REQUIRE_CLIENT_CERTIFICATE_AND_VERIFY;

            auto creds = grpc::SslServerCredentials(ssl_opts);
            builder.AddListeningPort(grpc_address, creds);
            spdlog::info("✅ gRPC server configured to use mTLS.");
        }

        builder.RegisterService(&grpc_service);
        grpc_server_ptr = builder.BuildAndStart();

        if (!grpc_server_ptr) {
            spdlog::critical("🔥 gRPC server failed to start on {}", grpc_address);
            return 1;
        }

        // --- SET HEALTH STATUS TO SERVING ---
        auto health_service = grpc_server_ptr->GetHealthCheckService();
        if (health_service) {
            health_service->SetServingStatus("sentiric.llm.v1.LLMLocalService", true);
            health_service->SetServingStatus("", true); 
            spdlog::info("✅ gRPC Health Status set to SERVING. Ready for Gateway traffic.");
        } else {
            spdlog::warn("⚠️ gRPC Health Check Service could not be retrieved. Gateway discovery might fail.");
        }

        spdlog::info("📡 gRPC server listening on {}", grpc_address);

        http_server = std::make_shared<HttpServer>(engine, settings.host, settings.http_port, settings.http_threads);
        metrics_server = std::make_shared<MetricsServer>(settings.host, settings.metrics_port, *registry);
        
        grpc_thread = std::thread(&grpc::Server::Wait, grpc_server_ptr.get());
        http_thread = std::thread(&HttpServer::run, http_server);
        metrics_thread = std::thread(&MetricsServer::run, metrics_server);

        spdlog::info("✅ All servers started successfully");
        spdlog::info("📊 Metrics available at http://{}:{}/metrics", settings.host, settings.metrics_port);

        auto shutdown_future = shutdown_promise.get_future();
        
        shutdown_future.wait();
        
        spdlog::info("🛑 Shutting down servers...");
        
        if (health_service) {
            health_service->SetServingStatus("", false);
        }

        http_server->stop();
        metrics_server->stop();
        grpc_server_ptr->Shutdown(std::chrono::system_clock::now() + std::chrono::seconds(5));

        if (http_thread.joinable()) http_thread.join();
        if (metrics_thread.joinable()) metrics_thread.join();
        if (grpc_thread.joinable()) grpc_thread.join();

    } catch (const std::exception& e) {
        spdlog::critical("🔥 Fatal error: {}", e.what());
        if (http_server) http_server->stop();
        if (metrics_server) metrics_server->stop();
        if (grpc_server_ptr) grpc_server_ptr->Shutdown();
        return 1;
    }

    spdlog::info("👋 Service shut down gracefully.");
    return 0;
}
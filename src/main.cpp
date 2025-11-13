// src/main.cpp
#include "config.h"
#include "llm_engine.h"
#include "grpc_server.h"
#include "http_server.h"
#include "model_manager.h"
#include <thread>
#include <memory>
#include <csignal>
#include <future>
#include <grpcpp/grpcpp.h>
#include <grpcpp/security/server_credentials.h>
#include <fstream>
#include <sstream>

// GÜNCELLENDİ: Sadece gerekli spdlog başlıkları
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace {
    std::promise<void> shutdown_promise;
}

void signal_handler(int signal) {
    spdlog::warn("Signal {} received. Initiating graceful shutdown...", signal);
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

// GÜNCELLENDİ: json_sink.h bağımlılığı olmadan loglamayı yapılandırır.
void setup_logging() {
    const char* env_p = std::getenv("ENV");
    std::string env = env_p ? std::string(env_p) : "development";
    auto sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    
    if (env == "production") {
        // JSON formatını manuel olarak bir pattern ile belirliyoruz.
        // Bu, json_sink.h başlığına olan ihtiyacı ortadan kaldırır.
        sink->set_pattern(R"({"timestamp":"%Y-%m-%dT%T.%fZ","level":"%l","service":"llm-llama-service","message":"%v"})");
    } else {
        // Geliştirme: Renkli, okunabilir konsol logları
        sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
    }
    
    auto logger = std::make_shared<spdlog::logger>("llm-llama-service", sink);
    spdlog::set_default_logger(logger);
}


int main() {
    setup_logging();

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    auto settings = load_settings();
    spdlog::set_level(spdlog::level::from_str(settings.log_level));
    spdlog::info("Sentiric LLM Llama Service starting...");

    std::unique_ptr<grpc::Server> grpc_server_ptr;
    std::shared_ptr<HttpServer> http_server;
    std::thread http_thread;
    std::thread grpc_thread;

    try {
        auto engine = std::make_shared<LLMEngine>(settings);
        if (!engine->is_model_loaded()) {
            spdlog::critical("LLM Engine failed to initialize with a valid model. Shutting down.");
            return 1;
        }

        std::string grpc_address = settings.host + ":" + std::to_string(settings.grpc_port);
        GrpcServer grpc_service(engine);
        grpc::ServerBuilder builder;

        const char* ca_path = std::getenv("GRPC_TLS_CA_PATH");
        const char* cert_path = std::getenv("LLM_LLAMA_SERVICE_CERT_PATH");
        const char* key_path = std::getenv("LLM_LLAMA_SERVICE_KEY_PATH");

        if (!ca_path || !cert_path || !key_path) {
            spdlog::warn("gRPC TLS environment variables not set. Falling back to insecure credentials. THIS IS NOT FOR PRODUCTION.");
            builder.AddListeningPort(grpc_address, grpc::InsecureServerCredentials());
        } else {
            spdlog::info("Loading TLS credentials for gRPC server...");
            std::string root_ca = read_file(ca_path);
            std::string server_cert = read_file(cert_path);
            std::string server_key = read_file(key_path);

            grpc::SslServerCredentialsOptions::PemKeyCertPair pkcp = {server_key, server_cert};
            grpc::SslServerCredentialsOptions ssl_opts;
            ssl_opts.pem_root_certs = root_ca;
            ssl_opts.pem_key_cert_pairs.push_back(pkcp);
            ssl_opts.client_certificate_request = GRPC_SSL_REQUEST_AND_REQUIRE_CLIENT_CERTIFICATE_AND_VERIFY;

            auto creds = grpc::SslServerCredentials(ssl_opts);
            builder.AddListeningPort(grpc_address, creds);
            spdlog::info("gRPC server configured to use mTLS.");
        }

        builder.RegisterService(&grpc_service);
        grpc_server_ptr = builder.BuildAndStart();

        if (!grpc_server_ptr) {
            spdlog::critical("gRPC server failed to start on {}", grpc_address);
            return 1;
        }
        spdlog::info("🚀 gRPC server listening on {}", grpc_address);

        http_server = std::make_shared<HttpServer>(engine, settings.host, settings.http_port);
        
        grpc_thread = std::thread(&grpc::Server::Wait, grpc_server_ptr.get());
        http_thread = std::thread(&HttpServer::run, http_server);

        auto shutdown_future = shutdown_promise.get_future();
        shutdown_future.wait();
        
        spdlog::info("Shutting down servers...");
        http_server->stop();
        grpc_server_ptr->Shutdown(std::chrono::system_clock::now() + std::chrono::seconds(5));

        if (http_thread.joinable()) http_thread.join();
        if (grpc_thread.joinable()) grpc_thread.join();

    } catch (const std::exception& e) {
        spdlog::critical("Fatal error during service lifecycle: {}", e.what());
        if (http_server) http_server->stop();
        if (grpc_server_ptr) grpc_server_ptr->Shutdown();
        return 1;
    }

    spdlog::info("Service shut down gracefully.");
    return 0;
}
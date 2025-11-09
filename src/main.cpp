#include "config.h"
#include "llm_engine.h"
#include "grpc_server.h"
#include "http_server.h"
#include <thread>
#include <memory>
#include <csignal>
#include <spdlog/spdlog.h>
#include <future>

// Global değişkenler yerine promise/future kullanmak daha modern bir yaklaşımdır.
std::promise<void> shutdown_promise;

void signal_handler(int signal) {
    spdlog::warn("Signal {} received. Initiating graceful shutdown...", signal);
    try {
        shutdown_promise.set_value();
    } catch (const std::future_error& e) {
        // Zaten set edilmişse ignore et.
    }
}

int main(int argc, char **argv) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    auto settings = load_settings();
    spdlog::set_level(spdlog::level::from_str(settings.log_level));
    spdlog::info("Sentiric LLM Llama Service starting...");

    std::unique_ptr<grpc::Server> grpc_server_ptr;
    std::shared_ptr<HttpServer> http_server;

    try {
        auto engine = std::make_shared<LLMEngine>(settings);

        // gRPC ve HTTP sunucularını başlat
        grpc_server_ptr = start_grpc_server(engine, settings.grpc_port);
        http_server = std::make_shared<HttpServer>(engine, settings.http_port);

        if (!grpc_server_ptr) {
             spdlog::critical("Failed to start gRPC server.");
             return 1;
        }

        std::thread grpc_thread(&grpc::Server::Wait, grpc_server_ptr.get());
        std::thread http_thread(&HttpServer::run, http_server.get());

        // Kapatma sinyali gelene kadar bekle
        auto shutdown_future = shutdown_promise.get_future();
        shutdown_future.wait();
        
        // Sunucuları kapat
        spdlog::info("Shutting down servers...");
        http_server->stop();
        grpc_server_ptr->Shutdown();

        // Thread'lerin bitmesini bekle
        http_thread.join();
        grpc_thread.join();

    } catch (const std::exception& e) {
        spdlog::critical("Fatal error during service lifecycle: {}", e.what());
        if (http_server) http_server->stop();
        if (grpc_server_ptr) grpc_server_ptr->Shutdown();
        return 1;
    }

    spdlog::info("Service shut down gracefully.");
    return 0;
}
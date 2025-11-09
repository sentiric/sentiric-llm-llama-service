#include "config.h"
#include "llm_engine.h"
#include "grpc_server.h"
#include "http_server.h"
#include <thread>
#include <memory>
#include <csignal>
#include <spdlog/spdlog.h>
#include <future>
#include <grpcpp/grpcpp.h> // grpc::Server için tam tanım

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

// DÜZELTME: Bu fonksiyonun run_grpc_server'dan ayrı olması daha temiz bir tasarım.
// run_grpc_server thread içinde çalışırken, bu fonksiyon sunucuyu oluşturup başlatır ve pointer'ı döndürür.
std::unique_ptr<grpc::Server> start_grpc_server(std::shared_ptr<LLMEngine> engine, int port) {
    std::string address = "0.0.0.0:" + std::to_string(port);
    // Servisi heap'te oluşturmak, scope dışına çıktığında silinmesini önler.
    auto service = new GrpcServer(engine); 
    
    grpc::ServerBuilder builder;
    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    builder.RegisterService(service);

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    if (!server) {
        spdlog::critical("gRPC server failed to start on {}", address);
        return nullptr;
    }
    spdlog::info("🚀 gRPC server listening on {}", address);
    return server;
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
        // DÜZELTME: start_grpc_server artık çağrılıyor
        grpc_server_ptr = start_grpc_server(engine, settings.grpc_port);
        http_server = std::make_shared<HttpServer>(engine, settings.http_port);

        if (!grpc_server_ptr) {
             spdlog::critical("Failed to start gRPC server.");
             return 1;
        }

        std::thread grpc_thread(&grpc::Server::Wait, grpc_server_ptr.get());
        std::thread http_thread(&HttpServer::run, http_server);

        // Kapatma sinyali gelene kadar bekle
        auto shutdown_future = shutdown_promise.get_future();
        shutdown_future.wait();
        
        // Sunucuları kapat
        spdlog::info("Shutting down servers...");
        http_server->stop();
        // Shutdown'a bir deadline vermek, takılı kalmasını önler.
        grpc_server_ptr->Shutdown(std::chrono::system_clock::now() + std::chrono::seconds(5));


        // Thread'lerin bitmesini bekle
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
#include "config.h"
#include "llm_engine.h"
#include "http_server.h"
#include "spdlog/spdlog.h"
#include <thread>
#include <csignal>

std::atomic<bool> shutdown_requested{false};

void signal_handler(int signal) {
    spdlog::warn("Signal {} received. Initiating shutdown...", signal);
    shutdown_requested = true;
}

int main(int argc, char **argv) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    auto settings = load_settings();
    spdlog::set_level(spdlog::level::from_str(settings.log_level));
    spdlog::info("Sentiric LLM Llama Service starting...");

    try {
        auto engine = std::make_shared<LLMEngine>(settings);
        auto http_server = std::make_shared<HttpServer>(engine, settings.http_port);
        
        spdlog::info("✅ Servis başlatıldı. Model hazır: {}", engine->is_model_loaded());
        spdlog::info("🌐 HTTP Health Check: http://localhost:{}/health", settings.http_port);
        
        // HTTP server'ı thread'de başlat
        std::thread http_thread([http_server]() {
            http_server->run();
        });
        
        // Ana döngü
        while (!shutdown_requested) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        http_server->stop();
        if (http_thread.joinable()) {
            http_thread.join();
        }
        
    } catch (const std::exception& e) {
        spdlog::critical("Fatal error: {}", e.what());
        return 1;
    }

    spdlog::info("Service shut down gracefully.");
    return 0;
}
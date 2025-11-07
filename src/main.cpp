#include "config.h"
#include "llm_engine.h"
// --- DEĞİŞİKLİK: Include yolu artık daha temiz ---
#include "grpc_server.h"
#include "http_server.h"
#include <thread>
#include <memory>
#include <csignal>

volatile sig_atomic_t g_signal_status;

void signal_handler(int signal) {
    g_signal_status = signal;
}

int main(int argc, char **argv) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    auto settings = load_settings();
    spdlog::set_level(spdlog::level::from_str(settings.log_level));
    spdlog::info("Sentiric LLM Llama Service starting...");

    try {
        auto engine = std::make_shared<LLMEngine>(settings);

        std::string grpc_address = "0.0.0.0:" + std::to_string(settings.grpc_port);
        std::thread grpc_thread(run_grpc_server, engine, grpc_address);

        std::thread http_thread(run_http_server, engine, "0.0.0.0", settings.http_port);

        // Ana thread'in sonlanmasını engelle
        grpc_thread.join();
        http_thread.join();

    } catch (const std::exception& e) {
        spdlog::critical("Fatal error during service initialization: {}", e.what());
        return 1;
    }

    spdlog::info("Service shutting down.");
    return 0;
}
#include "http_server.h"
#include "spdlog/spdlog.h"
#include "nlohmann/json.hpp"

using json = nlohmann::json;

HttpServer::HttpServer(std::shared_ptr<LLMEngine> engine, int port)
    : engine_(std::move(engine)), port_(port) {
    
    svr_.Get("/health", [this](const httplib::Request &, httplib::Response &res) {
        bool model_ready = engine_->is_model_loaded();
        int status_code = model_ready ? 200 : 503;

        json response_body;
        response_body["status"] = model_ready ? "healthy" : "unhealthy";
        response_body["model_ready"] = model_ready;
        response_body["engine"] = "llama.cpp";

        res.set_content(response_body.dump(), "application/json");
        res.status = status_code;
    });
}

void HttpServer::run() {
    spdlog::info("📊 HTTP health server listening on 0.0.0.0:{}", port_);
    svr_.listen("0.0.0.0", port_);
}

void HttpServer::stop() {
    if (svr_.is_running()) {
        svr_.stop();
        spdlog::info("HTTP server stopped.");
    }
}

void run_http_server_thread(std::shared_ptr<HttpServer> server) {
    if (server) {
        server->run();
    }
}
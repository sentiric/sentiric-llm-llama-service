#include "http_server.h"
#include "spdlog/spdlog.h"
#include "httplib.h"
#include "nlohmann/json.hpp"

using json = nlohmann::json;

void run_http_server(std::shared_ptr<LLMEngine> engine, const std::string& host, int port) {
    httplib::Server svr;

    svr.Get("/health", [engine](const httplib::Request &, httplib::Response &res) {
        bool model_ready = engine->is_model_loaded();
        int status_code = model_ready ? 200 : 503;

        json response_body;
        response_body["status"] = model_ready ? "healthy" : "unhealthy";
        response_body["model_ready"] = model_ready;
        response_body["engine"] = "llama.cpp";

        res.set_content(response_body.dump(2), "application/json");
        res.status = status_code;
    });

    spdlog::info("📊 HTTP health server listening on {}:{}", host, port);
    if (!svr.listen(host.c_str(), port)) {
        spdlog::critical("HTTP server failed to start on port {}", port);
    }
}
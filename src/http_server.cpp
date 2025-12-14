#include "http_server.h"
#include "spdlog/spdlog.h"
#include <sstream>
#include <prometheus/text_serializer.h>

// --- MetricsServer ---
MetricsServer::MetricsServer(const std::string& host, int port, prometheus::Registry& registry)
    : host_(host), port_(port), registry_(registry) {
    svr_.Get("/metrics", [this](const httplib::Request &, httplib::Response &res) {
        prometheus::TextSerializer serializer;
        auto collected_metrics = this->registry_.Collect();
        std::stringstream ss;
        serializer.Serialize(ss, collected_metrics);
        res.set_content(ss.str(), "text/plain; version=0.0.4");
    });
}
void MetricsServer::run() {
    spdlog::info("⚪ Prometheus metrics server listening on {}:{}", host_, port_);
    svr_.listen(host_.c_str(), port_);
}
void MetricsServer::stop() { if (svr_.is_running()) svr_.stop(); }
void run_metrics_server_thread(std::shared_ptr<MetricsServer> server) { if (server) server->run(); }

// --- HttpServer ---
HttpServer::HttpServer(std::shared_ptr<LLMEngine> engine, const std::string& host, int port)
    : engine_(std::move(engine)), host_(host), port_(port) {
    
    // Controller başlatma
    chat_controller_ = std::make_unique<ChatController>(engine_);
    model_controller_ = std::make_unique<ModelController>(engine_);
    system_controller_ = std::make_unique<SystemController>(engine_);
      
    const char* mount_point = "/";
    const char* base_dir = "./studio-v2"; 
    if (!svr_.set_mount_point(mount_point, base_dir)) {
        spdlog::error("UI directory '{}' not found.", base_dir);
    }
    
    // Global Logger
    svr_.set_logger([](const httplib::Request& req, const httplib::Response& res) {
        spdlog::debug("HTTP {} {} - Status: {}", req.method, req.path, res.status);
    });

    // Global CORS
    auto set_cors = [](const httplib::Request &, httplib::Response &res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        res.status = 204;
    };

    // --- SYSTEM ENDPOINTS ---
    svr_.Get("/health", [this](const httplib::Request &req, httplib::Response &res) {
        system_controller_->handle_health(req, res);
    });
    
    svr_.Get("/v1/hardware/config", [this](const httplib::Request &req, httplib::Response &res) {
        system_controller_->handle_get_hardware_config(req, res);
    });
    
    svr_.Post("/v1/hardware/config", [this](const httplib::Request &req, httplib::Response &res) {
        system_controller_->handle_post_hardware_config(req, res);
    });
    
    svr_.Get("/v1/ui/layout", [this](const httplib::Request &req, httplib::Response &res) {
        system_controller_->handle_ui_layout(req, res);
    });

    // --- MODEL ENDPOINTS ---
    svr_.Get("/v1/profiles", [this](const httplib::Request &req, httplib::Response &res) {
        model_controller_->handle_get_profiles(req, res);
    });

    svr_.Get("/v1/models", [this](const httplib::Request &req, httplib::Response &res) {
        model_controller_->handle_get_models(req, res);
    });

    svr_.Post("/v1/models/switch", [this](const httplib::Request &req, httplib::Response &res) {
        model_controller_->handle_switch_model(req, res);
    });

    // --- CHAT ENDPOINTS ---
    svr_.Post("/v1/chat/completions", [this](const httplib::Request &req, httplib::Response &res) {
        chat_controller_->handle_chat_completions(req, res);
    });

    // --- STATIC CONTENT ---
    svr_.Get(R"(/context/(.+))", [this](const httplib::Request &req, httplib::Response &res) {
        system_controller_->handle_static_context(req, res);
    });

    // --- OPTIONS HANDLERS ---
    svr_.Options("/v1/chat/completions", set_cors);
    svr_.Options("/v1/models", set_cors);
    svr_.Options("/v1/models/switch", set_cors);
    svr_.Options("/v1/profiles", set_cors);
    svr_.Options("/v1/hardware/config", set_cors);
}

void HttpServer::run() {
    spdlog::info("📊 HTTP sunucusu (Health + Studio) {}:{} adresinde dinleniyor.", host_, port_);
    svr_.listen(host_.c_str(), port_);
}
void HttpServer::stop() { if (svr_.is_running()) svr_.stop(); }
void run_http_server_thread(std::shared_ptr<HttpServer> server) { if (server) server->run(); }
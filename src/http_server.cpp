#include "http_server.h"
#include "spdlog/spdlog.h"
#include "nlohmann/json.hpp"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <vector>
#include <prometheus/text_serializer.h>
#include <chrono>
#include <algorithm>

namespace fs = std::filesystem;
using json = nlohmann::json;

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
    
    // YENİ: Controller başlatma
    chat_controller_ = std::make_unique<ChatController>(engine_);
      
    const char* mount_point = "/";
    const char* base_dir = "./studio-v2"; 
    if (!svr_.set_mount_point(mount_point, base_dir)) {
        spdlog::error("UI directory '{}' not found.", base_dir);
    }
    svr_.set_logger([](const httplib::Request& req, const httplib::Response& res) {
        spdlog::debug("HTTP {} {} - Status: {}", req.method, req.path, res.status);
    });

    // --- API: Hardware Config ---
    svr_.Get("/v1/hardware/config", [this](const httplib::Request &, httplib::Response &res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        json config = engine_->get_settings().to_json();
        res.set_content(config.dump(), "application/json");
    });

    svr_.Post("/v1/hardware/config", [this](const httplib::Request &req, httplib::Response &res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        try {
            json body = json::parse(req.body);
            int gpu_layers = body.value("gpu_layers", -1);
            int ctx_size = body.value("context_size", 4096);
            bool kv_offload = body.value("kv_offload", true);

            bool success = engine_->update_hardware_config(gpu_layers, ctx_size, kv_offload);
            
            if (success) {
                res.status = 200;
                res.set_content(json({{"status", "success"}}).dump(), "application/json");
            } else {
                res.status = 500;
                res.set_content(json({{"status", "error"}, {"message", "Hardware update failed"}}).dump(), "application/json");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json({{"error", e.what()}}).dump(), "application/json");
        }
    });

    // --- API: UI Layout (Omni-Studio v3) ---
    svr_.Get("/v1/ui/layout", [this](const httplib::Request &, httplib::Response &res) {
        const auto& s = engine_->get_settings();
        json layout_schema = {
            {"panels", {
                {"settings", {
                    {
                        {"type", "chip-group"},
                        {"id", "persona-chips"},
                        {"label", "PERSONA"},
                        {"options", {
                            {{"label", "🇹🇷 Asistan"}, {"value", "default"}, {"active", true}},
                            {{"label", "💻 Dev"}, {"value", "coder"}},
                            {{"label", "🤖 R1"}, {"value", "reasoning"}}
                        }}
                    },
                    {
                        {"type", "segmented"},
                        {"id", "reasoning-level"},
                        {"label", "AKIL YÜRÜTME"},
                        {"options", {
                            {{"label", "Kapalı"}, {"value", "none"}, {"active", true}},
                            {{"label", "Düşük"}, {"value", "low"}},
                            {{"label", "Yüksek"}, {"value", "high"}}
                        }}
                    },
                    {
                        {"type", "hardware-slider"},
                        {"id", "gpuLayers"},
                        {"label", "GPU Layers"},
                        {"display_id", "gpuVal"},
                        {"properties", { {"min", 0}, {"max", 100}, {"step", 1}, {"value", s.n_gpu_layers} }}
                    },
                    {
                        {"type", "hardware-slider"},
                        {"id", "ctxSize"},
                        {"label", "Context Size"},
                        {"display_id", "ctxVal"},
                        {"properties", { {"min", 1024}, {"max", 32768}, {"step", 1024}, {"value", s.context_size} }}
                    },
                    {
                        {"type", "slider"},
                        {"id", "tempInput"},
                        {"label", "Sıcaklık"},
                        {"display_id", "tempVal"},
                        {"properties", { {"min", 0.0}, {"max", 2.0}, {"step", 0.1}, {"value", 0.7} }}
                    }
                }}
            }}
        };
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_content(layout_schema.dump(), "application/json");
    });
    
    svr_.Get("/v1/profiles", [](const httplib::Request &, httplib::Response &res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        try {
            std::ifstream f("profiles.json");
            if (!f.is_open()) {
                 f.open("models/profiles.json");
            }
            json profiles_json = json::parse(f);
            res.set_content(profiles_json.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(json({{"error", "Could not read profiles.json"}}).dump(), "application/json");
        }
    });

    svr_.Get("/v1/models", [this](const httplib::Request &, httplib::Response &res) {
        const auto& current_settings = engine_->get_settings();
        json response_body = {
            {"object", "list"},
            {"data", {{
                {"id", current_settings.model_id.empty() ? "local-model" : current_settings.model_id},
                {"object", "model"},
                {"created", std::time(nullptr)},
                {"owned_by", "system"},
                {"active", true},
                {"profile", current_settings.profile_name}
            }}}
        };
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_content(response_body.dump(), "application/json");
    });

    svr_.Post("/v1/models/switch", [this](const httplib::Request &req, httplib::Response &res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        try {
            json body = json::parse(req.body);
            std::string profile = body.value("profile", "");
            
            if (profile.empty()) {
                res.status = 400; res.set_content(json({{"error", "Profile name required"}}).dump(), "application/json"); return;
            }

            spdlog::warn("⚠️ API requested model switch to profile: {}", profile);
            bool success = engine_->reload_model(profile);
            
            if (success) {
                res.status = 200;
                res.set_content(json({{"status", "success"}, {"active_profile", profile}}).dump(), "application/json");
            } else {
                res.status = 500;
                res.set_content(json({{"status", "error"}, {"message", "Model reload failed. Profile not found or download error."}}).dump(), "application/json");
            }
        } catch(const std::exception& e) {
            res.status = 400; res.set_content(json({{"error", e.what()}}).dump(), "application/json");
        }
    });

    svr_.Get("/health", [this](const httplib::Request &, httplib::Response &res) {
        bool model_ready = engine_->is_model_loaded();
        size_t active_ctx = 0;
        size_t total_ctx = 0;
        
        if (model_ready) {
            active_ctx = engine_->get_context_pool().get_active_count();
            total_ctx = engine_->get_context_pool().get_total_count();
        }

        json response_body = {
            {"status", model_ready ? "healthy" : "loading"},
            {"model_ready", model_ready},
            {"current_profile", engine_->get_settings().profile_name},
            {"capacity", {
                {"active", active_ctx},
                {"total", total_ctx},
                {"available", total_ctx - active_ctx}
            }},
            {"timestamp", std::time(nullptr)}
        };
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_content(response_body.dump(), "application/json");
        res.status = model_ready ? 200 : 503;
    });

    // YENİ: Delegate to Controller
    svr_.Post("/v1/chat/completions", [this](const httplib::Request &req, httplib::Response &res) {
        chat_controller_->handle_chat_completions(req, res);
    });
    
    svr_.Get(R"(/context/(.+))", [](const httplib::Request &req, httplib::Response &res) {
        std::string filename = req.matches[1];
        fs::path file_path = fs::path("examples") / filename;
        if (filename.find("..") != std::string::npos) { res.status = 400; return; }
        std::ifstream file(file_path);
        if (file) { 
            std::stringstream buffer; buffer << file.rdbuf(); 
            res.set_header("Access-Control-Allow-Origin", "*");
            res.set_content(buffer.str(), "text/plain; charset=utf-8"); 
        } else { res.status = 404; }
    });

    auto set_cors = [](const httplib::Request &, httplib::Response &res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        res.status = 204;
    };
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
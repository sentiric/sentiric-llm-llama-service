#include "controllers/system_controller.h"
#include "spdlog/spdlog.h"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;
using json = nlohmann::json;

SystemController::SystemController(std::shared_ptr<LLMEngine> engine)
    : engine_(std::move(engine)) {}

void SystemController::handle_health(const httplib::Request &, httplib::Response &res) {
    res.set_header("Access-Control-Allow-Origin", "*");

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
    
    res.set_content(response_body.dump(), "application/json");
    res.status = model_ready ? 200 : 503;
}

void SystemController::handle_get_hardware_config(const httplib::Request &, httplib::Response &res) {
    res.set_header("Access-Control-Allow-Origin", "*");
    json config = engine_->get_settings().to_json();
    res.set_content(config.dump(), "application/json");
}

void SystemController::handle_post_hardware_config(const httplib::Request &req, httplib::Response &res) {
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
}

void SystemController::handle_ui_layout(const httplib::Request &, httplib::Response &res) {
    res.set_header("Access-Control-Allow-Origin", "*");
    const auto& s = engine_->get_settings();
    
    // UI Şeması (Layout Schema)
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
    
    res.set_content(layout_schema.dump(), "application/json");
}

void SystemController::handle_static_context(const httplib::Request &req, httplib::Response &res) {
    std::string filename = req.matches[1];
    res.set_header("Access-Control-Allow-Origin", "*");
    
    try {
        fs::path base_path = fs::canonical(fs::path("examples"));
        fs::path requested_path = fs::weakly_canonical(base_path / filename);

        // Security Check: Path Traversal Protection
        if (requested_path.string().find(base_path.string()) != 0) {
            spdlog::warn("🚨 Security Alert: Path traversal attempt detected. Input: {}", filename);
            res.status = 403;
            res.set_content("Access Denied", "text/plain");
            return;
        }

        if (fs::exists(requested_path) && fs::is_regular_file(requested_path)) {
            std::ifstream file(requested_path);
            if (file) { 
                std::stringstream buffer; 
                buffer << file.rdbuf(); 
                res.set_content(buffer.str(), "text/plain; charset=utf-8"); 
                return;
            }
        }
        res.status = 404;
    } catch (const std::exception& e) {
        spdlog::error("Error in static context handler: {}", e.what());
        res.status = 500;
    }
}
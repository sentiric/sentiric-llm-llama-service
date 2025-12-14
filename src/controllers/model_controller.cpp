#include "controllers/model_controller.h"
#include "spdlog/spdlog.h"
#include <fstream>
#include <ctime>

using json = nlohmann::json;

ModelController::ModelController(std::shared_ptr<LLMEngine> engine)
    : engine_(std::move(engine)) {}

void ModelController::handle_get_models(const httplib::Request &, httplib::Response &res) {
    res.set_header("Access-Control-Allow-Origin", "*");
    
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
    
    res.set_content(response_body.dump(), "application/json");
}

void ModelController::handle_switch_model(const httplib::Request &req, httplib::Response &res) {
    res.set_header("Access-Control-Allow-Origin", "*");
    
    try {
        json body = json::parse(req.body);
        std::string profile = body.value("profile", "");
        
        if (profile.empty()) {
            res.status = 400; 
            res.set_content(json({{"error", "Profile name required"}}).dump(), "application/json"); 
            return;
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
        res.status = 400; 
        res.set_content(json({{"error", e.what()}}).dump(), "application/json");
    }
}

void ModelController::handle_get_profiles(const httplib::Request &, httplib::Response &res) {
    res.set_header("Access-Control-Allow-Origin", "*");
    try {
        std::ifstream f("profiles.json");
        if (!f.is_open()) {
             f.open("models/profiles.json");
        }
        
        if (f.is_open()) {
            json profiles_json = json::parse(f);
            res.set_content(profiles_json.dump(), "application/json");
        } else {
            throw std::runtime_error("Profiles file not found");
        }
    } catch (const std::exception& e) {
        res.status = 500;
        res.set_content(json({{"error", "Could not read profiles.json"}}).dump(), "application/json");
    }
}
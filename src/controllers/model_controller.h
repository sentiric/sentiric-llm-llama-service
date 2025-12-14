#pragma once

#include "llm_engine.h"
#include "httplib.h"
#include "nlohmann/json.hpp"
#include <memory>
#include <string>

class ModelController {
public:
    explicit ModelController(std::shared_ptr<LLMEngine> engine);

    // Endpoint Handlers
    void handle_get_models(const httplib::Request& req, httplib::Response& res);
    void handle_switch_model(const httplib::Request& req, httplib::Response& res);
    void handle_get_profiles(const httplib::Request& req, httplib::Response& res);

private:
    std::shared_ptr<LLMEngine> engine_;
};
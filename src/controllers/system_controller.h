#pragma once

#include "llm_engine.h"
#include "httplib.h"
#include "nlohmann/json.hpp"
#include <memory>

class SystemController {
public:
    explicit SystemController(std::shared_ptr<LLMEngine> engine);

    void handle_health(const httplib::Request& req, httplib::Response& res);
    void handle_get_hardware_config(const httplib::Request& req, httplib::Response& res);
    void handle_post_hardware_config(const httplib::Request& req, httplib::Response& res);
    void handle_ui_layout(const httplib::Request& req, httplib::Response& res);
    void handle_static_context(const httplib::Request& req, httplib::Response& res);

private:
    std::shared_ptr<LLMEngine> engine_;
};
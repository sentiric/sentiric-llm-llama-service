#include "http_server.h"
#include "spdlog/spdlog.h"
#include "nlohmann/json.hpp"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <vector>
#include <prometheus/text_serializer.h>
#include <chrono>

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
      
    // Statik dosya sunumu (Studio UI)
    const char* mount_point = "/";
    const char* base_dir = "./studio"; 
    if (!svr_.set_mount_point(mount_point, base_dir)) {
        spdlog::error("UI directory '{}' not found.", base_dir);
    }
    svr_.set_logger([](const httplib::Request& req, const httplib::Response& res) {
        spdlog::debug("HTTP {} {} - Status: {}", req.method, req.path, res.status);
    });

    // --- Endpoint: Model Profillerini Listele ---
    svr_.Get("/v1/models", [this](const httplib::Request &, httplib::Response &res) {
        const auto& current_settings = engine_->get_settings();
        
        json models_data = json::array();
        
        // 1. Aktif Model
        models_data.push_back({
            {"id", current_settings.model_id.empty() ? "local-model" : current_settings.model_id},
            {"object", "model"},
            {"created", std::time(nullptr)},
            {"owned_by", "system"},
            {"active", true},
            {"profile", current_settings.profile_name}
        });

        // 2. Diğer Profiller (profiles.json'dan)
        try {
            std::ifstream f("models/profiles.json");
            json j = json::parse(f);
            if (j.contains("profiles")) {
                for (auto& [key, val] : j["profiles"].items()) {
                    if (key != current_settings.profile_name) {
                        models_data.push_back({
                            {"id", val.value("model_id", key)},
                            {"object", "model"},
                            {"created", std::time(nullptr)},
                            {"owned_by", "system"},
                            {"active", false},
                            {"profile", key}
                        });
                    }
                }
            }
        } catch(...) {}

        json response_body = {
            {"object", "list"},
            {"data", models_data}
        };
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_content(response_body.dump(), "application/json");
    });

    // --- Endpoint: Model Değiştir (Hot-Swap) ---
    svr_.Post("/v1/models/switch", [this](const httplib::Request &req, httplib::Response &res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        try {
            json body = json::parse(req.body);
            std::string profile = body.value("profile", "");
            
            if (profile.empty()) {
                res.status = 400; res.set_content(json({{"error", "Profile name required"}}).dump(), "application/json"); return;
            }

            spdlog::warn("⚠️ API requested model switch to profile: {}", profile);
            
            // Blocking Call (Reload bitene kadar bekler)
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

    // --- Endpoint: Health Check ---
    svr_.Get("/health", [this](const httplib::Request &, httplib::Response &res) {
        bool model_ready = engine_->is_model_loaded();
        size_t active_ctx = 0;
        size_t total_ctx = 0;
        
        // Engine pointer kontrolü (Crash prevention)
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

    // --- Endpoint: Chat Completions (Core Logic) ---
    svr_.Post("/v1/chat/completions", [this](const httplib::Request &req, httplib::Response &res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        
        try {
            if (!engine_->is_model_loaded()) {
                res.status = 503; 
                res.set_content(json({{"error", "Model is reloading or not ready"}}).dump(), "application/json"); 
                return;
            }

            json body = json::parse(req.body);
            sentiric::llm::v1::GenerateStreamRequest grpc_request;

            // OpenAI Message Format -> gRPC Request Parsing
            if (body.contains("messages") && body["messages"].is_array()) {
                const auto& messages = body["messages"];
                for (size_t i = 0; i < messages.size(); ++i) {
                    const auto& msg = messages[i];
                    std::string role = msg.value("role", "");
                    std::string content = msg.value("content", "");
                    if (role == "system") grpc_request.set_system_prompt(content);
                    else if (i == messages.size() - 1 && role == "user") grpc_request.set_user_prompt(content);
                    else {
                        auto* turn = grpc_request.add_history();
                        turn->set_role(role);
                        turn->set_content(content);
                    }
                }
            }
            auto* params = grpc_request.mutable_params();
            if (body.contains("max_tokens")) params->set_max_new_tokens(body["max_tokens"]);
            if (body.contains("temperature")) params->set_temperature(body["temperature"]);

            bool stream = body.value("stream", false);
            auto batched_request = std::make_shared<BatchedRequest>();
            batched_request->request = grpc_request;

            // Grammar / JSON Mode Support
            if (body.contains("response_format") && body["response_format"].value("type", "") == "json_object") {
                 // Basit JSON grammar
                 batched_request->grammar = R"(root ::= object; value ::= object | array | string | number | ("true" | "false" | "null") ws; object ::= "{" ws (string ":" ws value ("," ws string ":" ws value)*)? "}" ws; array ::= "[" ws (value ("," ws value)*)? "]" ws; string ::= "\"" ([^"\\\x7F\x00-\x1F] | "\\" (["\\/bfnrt] | "u" [0-9a-fA-F]{4}))* "\"" ws; number ::= ("-"? ([0-9] | [1-9] [0-9]*)) ("." [0-9]+)? ([eE] [-+]? [0-9]+)? ws; ws ::= [ \t\n\r]*)";
            } else if (body.contains("grammar")) {
                batched_request->grammar = body["grammar"].get<std::string>();
            }

            // GÜVENLİK: Asla detach kullanma. Batcher queue'ya ekle.
            auto completion_future = engine_->get_batcher()->add_request(batched_request);

            if (stream) {
                // Streaming Response
                res.set_chunked_content_provider("text/event-stream",
                    [batched_request](size_t, httplib::DataSink &sink) {
                        // İstemci bağlantıyı kopardıysa dur
                        batched_request->should_stop_callback = [&sink]() { return !sink.is_writable(); };

                        while (!batched_request->is_finished || !batched_request->token_queue.empty()) {
                            std::string token;
                            if (batched_request->token_queue.wait_and_pop(token, 50)) {
                                json chunk;
                                chunk["id"] = "chatcmpl-" + std::to_string(std::time(nullptr));
                                chunk["object"] = "chat.completion.chunk";
                                chunk["created"] = std::time(nullptr);
                                chunk["model"] = "sentiric-llm";
                                chunk["choices"][0]["index"] = 0;
                                chunk["choices"][0]["delta"]["content"] = token;
                                
                                std::string data = "data: " + chunk.dump() + "\n\n";
                                if (!sink.write(data.c_str(), data.length())) return false;
                            }
                        }
                        sink.write("data: [DONE]\n\n", 12);
                        sink.done();
                        return true;
                    });
            } else {
                 // Non-Streaming Response (Blocking wait)
                 // Batcher thread'i işi bitirene kadar bekle. 
                 completion_future.wait();
                 
                 std::string full_response;
                 while (!batched_request->token_queue.empty()) {
                     std::string t;
                     if(batched_request->token_queue.wait_and_pop(t, 0)) full_response += t;
                 }
                 
                 json response_json;
                 response_json["id"] = "chatcmpl-" + std::to_string(std::time(nullptr));
                 response_json["object"] = "chat.completion";
                 response_json["created"] = std::time(nullptr);
                 response_json["model"] = "sentiric-llm";
                 response_json["choices"][0]["index"] = 0;
                 response_json["choices"][0]["message"]["role"] = "assistant";
                 response_json["choices"][0]["message"]["content"] = full_response;
                 response_json["choices"][0]["finish_reason"] = batched_request->finish_reason;
                 response_json["usage"]["prompt_tokens"] = batched_request->prompt_tokens;
                 response_json["usage"]["completion_tokens"] = batched_request->completion_tokens;
                 response_json["usage"]["total_tokens"] = batched_request->prompt_tokens + batched_request->completion_tokens;

                 res.set_content(response_json.dump(), "application/json");
            }

        } catch (const std::exception& e) {
            spdlog::error("HTTP handler error: {}", e.what());
            res.status = 400;
            res.set_content(json({{"error", e.what()}}).dump(), "application/json");
        }
    });
    
    // --- Context Dosyaları Listeleme (Aynı) ---
    svr_.Get("/contexts", [](const httplib::Request &, httplib::Response &res) {
        json context_list = json::array();
        try {
            if (fs::exists("examples") && fs::is_directory("examples")) {
                for (const auto & entry : fs::directory_iterator("examples")) {
                    if (entry.is_regular_file() && entry.path().extension() == ".txt") {
                        context_list.push_back(entry.path().filename().string());
                    }
                }
            }
        } catch (...) {}
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_content(context_list.dump(), "application/json");
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

    // --- CORS Preflight Options ---
    auto set_cors = [](const httplib::Request &, httplib::Response &res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        res.status = 204;
    };
    svr_.Options("/v1/chat/completions", set_cors);
    svr_.Options("/v1/models", set_cors);
    svr_.Options("/v1/models/switch", set_cors);
}

void HttpServer::run() {
    spdlog::info("📊 HTTP sunucusu (Health + Studio) {}:{} adresinde dinleniyor.", host_, port_);
    svr_.listen(host_.c_str(), port_);
}
void HttpServer::stop() { if (svr_.is_running()) svr_.stop(); }
void run_http_server_thread(std::shared_ptr<HttpServer> server) { if (server) server->run(); }
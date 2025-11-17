#include "http_server.h"
#include "spdlog/spdlog.h"
#include "nlohmann/json.hpp"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <vector>
#include <prometheus/text_serializer.h>

namespace fs = std::filesystem;
using json = nlohmann::json;

// ==============================================================================
//  MetricsServer Sınıfı Implementasyonu
// ==============================================================================

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

void MetricsServer::stop() {
    if (svr_.is_running()) {
        svr_.stop();
        spdlog::info("Prometheus metrics server stopped.");
    }
}

void run_metrics_server_thread(std::shared_ptr<MetricsServer> server) {
    if (server) {
        server->run();
    }
}


// ==============================================================================
//  HttpServer Sınıfı Implementasyonu (Geliştirme Stüdyosu)
// ==============================================================================

HttpServer::HttpServer(std::shared_ptr<LLMEngine> engine, const std::string& host, int port)
    : engine_(std::move(engine)), host_(host), port_(port) {
    
    const char* mount_point = "/";
    const char* base_dir = "./web";
    if (!svr_.set_mount_point(mount_point, base_dir)) {
        spdlog::error("UI directory '{}' not found. The UI will not be available.", base_dir);
    }

    svr_.Get("/health", [this](const httplib::Request &, httplib::Response &res) {
        bool model_ready = engine_->is_model_loaded();
        json response_body;
        response_body["status"] = model_ready ? "healthy" : "unhealthy";
        response_body["model_ready"] = model_ready;
        response_body["engine"] = "llama.cpp";
        res.set_content(response_body.dump(), "application/json");
        res.status = model_ready ? 200 : 503;
    });

    svr_.Get("/web/contexts", [](const httplib::Request &, httplib::Response &res) {
        json context_list = json::array();
        try {
            if (fs::exists("examples") && fs::is_directory("examples")) {
                for (const auto & entry : fs::directory_iterator("examples")) {
                    if (entry.is_regular_file() && entry.path().extension() == ".txt") {
                        context_list.push_back(entry.path().filename().string());
                    }
                }
            }
        } catch (const fs::filesystem_error& e) { spdlog::error("RAG context directory ('examples') could not be read: {}", e.what()); }
        res.set_content(context_list.dump(), "application/json");
    });

    svr_.Get(R"(/web/context/(.+))", [](const httplib::Request &req, httplib::Response &res) {
        std::string filename = req.matches[1];
        fs::path file_path = fs::path("examples") / filename;
        if (file_path.string().find("..") != std::string::npos) { 
            res.status = 400; 
            res.set_content("Invalid filename.", "text/plain"); 
            return; 
        }
        std::ifstream file(file_path);
        if (file) { 
            std::stringstream buffer; 
            buffer << file.rdbuf(); 
            res.set_content(buffer.str(), "text/plain; charset=utf-8"); 
        } 
        else { 
            res.status = 404; 
            res.set_content("Context file not found: " + filename, "text/plain"); 
        }
    });

    svr_.Post("/web/generate", [this](const httplib::Request &req, httplib::Response &res) {
        try {
            json body = json::parse(req.body);
            sentiric::llm::v1::LLMLocalServiceGenerateStreamRequest request;
            request.set_user_prompt(body.value("user_prompt", ""));
            request.set_system_prompt(body.value("system_prompt", "You are a helpful assistant."));
            if (body.contains("rag_context") && !body["rag_context"].is_null()) { request.set_rag_context(body.value("rag_context", "")); }
            if (body.contains("history") && body["history"].is_array()) { for (const auto& turn_json : body["history"]) { auto* turn = request.add_history(); turn->set_role(turn_json.value("role", "")); turn->set_content(turn_json.value("content", "")); } }
            if (body.contains("params") && body["params"].is_object()) { auto* params = request.mutable_params(); const auto& p = body["params"]; if (p.contains("max_new_tokens")) params->set_max_new_tokens(p["max_new_tokens"]); if (p.contains("temperature")) params->set_temperature(p["temperature"]); }

            res.set_chunked_content_provider("text/plain; charset=utf-8",
                [this, request](size_t, httplib::DataSink &sink) {
                    int32_t prompt_tokens = 0, completion_tokens = 0;
                    try {
                        engine_->generate_stream(
                            request,
                            [&sink](const std::string& token) { sink.write(token.c_str(), token.length()); },
                            [&sink]() { return !sink.is_writable(); },
                            prompt_tokens,
                            completion_tokens
                        );
                        json metadata;
                        metadata["prompt_tokens"] = prompt_tokens;
                        metadata["completion_tokens"] = completion_tokens;
                        std::string metadata_str = "<<METADATA>>" + metadata.dump();
                        sink.write(metadata_str.c_str(), metadata_str.length());
                    } catch (const std::exception& e) {
                        spdlog::error("HTTP stream generation error: {}", e.what());
                        std::string error_msg = "<<METADATA>>{\"error\":\"" + std::string(e.what()) + "\"}";
                        sink.write(error_msg.c_str(), error_msg.length());
                    }
                    sink.done();
                    return true;
                }
            );
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(std::string("Unexpected server error: ") + e.what(), "text/plain");
        }
    });
}

void HttpServer::run() {
    spdlog::info("📊 HTTP sunucusu (Health + Geliştirme Stüdyosu) {}:{} adresinde dinleniyor.", host_, port_);
    svr_.listen(host_.c_str(), port_);
}

void HttpServer::stop() {
    if (svr_.is_running()) {
        svr_.stop();
        spdlog::info("HTTP sunucusu durduruldu.");
    }
}

void run_http_server_thread(std::shared_ptr<HttpServer> server) {
    if (server) {
        server->run();
    }
}
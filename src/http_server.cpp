
// src/http_server.cpp
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
//  HttpServer Sınıfı Implementasyonu (Geliştirme Stüdyosu + OpenAI API)
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
        // This endpoint can be refactored to use the same logic as the OpenAI endpoint for consistency
    });

    // OpenAI-COMPATIBLE ENDPOINT
    svr_.Post("/v1/chat/completions", [this](const httplib::Request &req, httplib::Response &res) {
        try {
            json body = json::parse(req.body);
            sentiric::llm::v1::LLMLocalServiceGenerateStreamRequest grpc_request;


            if (body.contains("messages") && body["messages"].is_array()) {
                const auto& messages = body["messages"];
                if (messages.empty()) throw std::invalid_argument("'messages' array cannot be empty.");

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
            } else {
                throw std::invalid_argument("Request must contain a 'messages' array.");
            }
            
            auto* params = grpc_request.mutable_params();
            if (body.contains("max_tokens")) params->set_max_new_tokens(body["max_tokens"]);
            if (body.contains("temperature")) params->set_temperature(body["temperature"]);
            if (body.contains("top_p")) params->set_top_p(body["top_p"]);
            

            bool stream = body.value("stream", false);
            auto batched_request = std::make_shared<BatchedRequest>();
            batched_request->request = grpc_request;

            auto handle_request = [&]() {
                if (engine_->is_batching_enabled()) {
                    auto future = engine_->get_batcher()->add_request(batched_request);
                    future.get();
                } else {
                    engine_->process_single_request(batched_request);
                }
            };

            if (stream) {
                res.set_chunked_content_provider("application/json; charset=utf-8",
                    [this, batched_request, handle_request](size_t, httplib::DataSink &sink) {
                        batched_request->on_token_callback = [&sink](const std::string& token) -> bool {
                            json chunk;
                            chunk["id"] = "chatcmpl-mock-id-stream";
                            chunk["object"] = "chat.completion.chunk";
                            chunk["created"] = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
                            chunk["model"] = "llm-llama-service";
                            chunk["choices"][0]["index"] = 0;
                            chunk["choices"][0]["delta"]["content"] = token;
                            
                            std::string data = "data: " + chunk.dump() + "\n\n";
                            return sink.write(data.c_str(), data.length());
                        };
                        batched_request->should_stop_callback = [&sink]() { return !sink.is_writable(); };

                        try { handle_request(); } 
                        catch (...) { /* Errors are propagated via promise */ }

                        sink.write("data: [DONE]\n\n", 12);
                        sink.done();
                        return true;
                    });
            } else {
                 std::string full_response;
                 batched_request->on_token_callback = [&full_response](const std::string& token) -> bool {
                    full_response += token;
                    return true;
                 };
                 batched_request->should_stop_callback = []() { return false; };
                 
                 handle_request();

                 json response_json;
                 response_json["id"] = "chatcmpl-mock-id-nonstream";
                 response_json["object"] = "chat.completion";
                 response_json["created"] = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
                 response_json["model"] = "llm-llama-service";
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
            res.status = 400;
            res.set_content(json({{"error", {{"message", e.what(), "type", "invalid_request_error"}}}}).dump(), "application/json");
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
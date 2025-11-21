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

// --- STANDART JSON GRAMMAR (GBNF) ---
const std::string GENERIC_JSON_GBNF = R"(
root   ::= object
value  ::= object | array | string | number | ("true" | "false" | "null") ws
object ::=
  "{" ws (
    string ":" ws value
    ("," ws string ":" ws value)*
  )? "}" ws
array  ::=
  "[" ws (
    value
    ("," ws value)*
  )? "]" ws
string ::=
  "\"" (
    [^"\\\x7F\x00-\x1F] |
    "\\" (["\\/bfnrt] | "u" [0-9a-fA-F]{4})
  )* "\"" ws
number ::= ("-"? ([0-9] | [1-9] [0-9]*)) ("." [0-9]+)? ([eE] [-+]? [0-9]+)? ws
ws     ::= [ \t\n\r]*
)";

// MetricsServer Implementation
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
    if (svr_.is_running()) svr_.stop();
}
void run_metrics_server_thread(std::shared_ptr<MetricsServer> server) {
    if (server) server->run();
}

// HttpServer Implementation
HttpServer::HttpServer(std::shared_ptr<LLMEngine> engine, const std::string& host, int port)
    : engine_(std::move(engine)), host_(host), port_(port) {
      
    const char* mount_point = "/";
    const char* base_dir = "./studio"; 
    if (!svr_.set_mount_point(mount_point, base_dir)) {
        spdlog::error("UI directory '{}' not found.", base_dir);
    }

    svr_.set_logger([](const httplib::Request& req, const httplib::Response& res) {
        spdlog::debug("HTTP {} {} - Status: {}", req.method, req.path, res.status);
    });

    svr_.Get("/health", [this](const httplib::Request &, httplib::Response &res) {
        bool model_ready = engine_->is_model_loaded();
        json response_body = {{"status", model_ready ? "healthy" : "unhealthy"}, {"model_ready", model_ready}};
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_content(response_body.dump(), "application/json");
        res.status = model_ready ? 200 : 503;
    });

    // --- YENİ: OpenAI Uyumlu Models Endpoint'i ---
    svr_.Get("/v1/models", [this](const httplib::Request &, httplib::Response &res) {
        const auto& settings = engine_->get_settings();
        json model_card = {
            {"id", settings.model_id.empty() ? "local-model" : settings.model_id},
            {"object", "model"},
            {"created", std::time(nullptr)},
            {"owned_by", "sentiric-llm-service"}
        };
        json response_body = {
            {"object", "list"},
            {"data", {model_card}}
        };
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_content(response_body.dump(), "application/json");
    });

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

    // --- CHAT COMPLETION ENDPOINT ---
    svr_.Post("/v1/chat/completions", [this](const httplib::Request &req, httplib::Response &res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        
        try {
            json body = json::parse(req.body);
            sentiric::llm::v1::LLMLocalServiceGenerateStreamRequest grpc_request;

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

            // --- GRAMMAR ENTEGRASYONU ---
            // 1. OpenAI tarzı "json_object" isteği
            if (body.contains("response_format") && 
                body["response_format"].value("type", "") == "json_object") {
                batched_request->grammar = GENERIC_JSON_GBNF;
            } 
            // 2. Doğrudan "grammar" alanı (Raw GBNF)
            else if (body.contains("grammar")) {
                batched_request->grammar = body["grammar"].get<std::string>();
            }

            // 1. İsteği Engine/Batcher thread'ine gönder (Producer)
            if (engine_->is_batching_enabled()) {
                engine_->get_batcher()->add_request(batched_request);
            } else {
                 std::thread([this, batched_request](){
                     engine_->process_single_request(batched_request);
                     batched_request->is_finished = true; 
                 }).detach();
            }

            if (stream) {
                // 2. Tüketici Döngüsü (Consumer)
                res.set_chunked_content_provider("text/event-stream",
                    [batched_request](size_t, httplib::DataSink &sink) {
                        
                        batched_request->should_stop_callback = [&sink]() { return !sink.is_writable(); };

                        while (!batched_request->is_finished || !batched_request->token_queue.empty()) {
                            std::string token;
                            if (batched_request->token_queue.wait_and_pop(token, 50)) {
                                json chunk;
                                chunk["id"] = "chatcmpl-stream";
                                chunk["object"] = "chat.completion.chunk";
                                chunk["created"] = std::time(nullptr);
                                chunk["model"] = "llm-llama-service";
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
                 // Non-streaming: Bekle ve topla
                 std::string full_response;
                 while (!batched_request->is_finished || !batched_request->token_queue.empty()) {
                     std::string t;
                     if(batched_request->token_queue.wait_and_pop(t, 50)) full_response += t;
                 }

                 json response_json;
                 response_json["choices"][0]["message"]["content"] = full_response;
                 res.set_content(response_json.dump(), "application/json");
            }

        } catch (const std::exception& e) {
            spdlog::error("HTTP handler error: {}", e.what());
            res.status = 400;
            res.set_content(json({{"error", e.what()}}).dump(), "application/json");
        }
    });

    svr_.Options("/v1/chat/completions", [](const httplib::Request &, httplib::Response &res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        res.status = 204;
    });
    
    // Model endpoint için OPTIONS
    svr_.Options("/v1/models", [](const httplib::Request &, httplib::Response &res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        res.status = 204;
    });
}

void HttpServer::run() {
    spdlog::info("📊 HTTP sunucusu (Health + Studio) {}:{} adresinde dinleniyor.", host_, port_);
    svr_.listen(host_.c_str(), port_);
}
void HttpServer::stop() {
    if (svr_.is_running()) svr_.stop();
}
void run_http_server_thread(std::shared_ptr<HttpServer> server) {
    if (server) server->run();
}
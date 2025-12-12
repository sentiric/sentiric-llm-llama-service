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

// --- Helper Functions ---

// 1. UTF-8 Kontrolü (Multi-byte karakterlerin bölünmesini önler)
bool has_incomplete_utf8_suffix(const std::string& str) {
    if (str.empty()) return false;
    size_t len = str.length();
    for (size_t i = 1; i <= 4 && i <= len; ++i) {
        unsigned char c = static_cast<unsigned char>(str[len - i]);
        if ((c & 0xC0) == 0x80) continue;
        if ((c & 0x80) == 0) return false;
        size_t char_len = 0;
        if ((c & 0xE0) == 0xC0) char_len = 2;
        else if ((c & 0xF0) == 0xE0) char_len = 3;
        else if ((c & 0xF8) == 0xF0) char_len = 4;
        else return false;
        if (i < char_len) return true;
        return false;
    }
    return false;
}

// 2. Token Sanitizer (Görünmez kontrol karakterlerini temizler)
std::string sanitize_token(const std::string& input) {
    std::string clean = input;
    clean.erase(std::remove_if(clean.begin(), clean.end(), [](unsigned char c) {
        return (c < 32 && c != 9 && c != 10 && c != 13) || c == 127;
    }), clean.end());
    return clean;
}

// 3. Reasoning Instruction Generator (Yeni Özellik)
std::string get_reasoning_instruction(const std::string& level) {
    if (level == "low") {
        return "\n[TALİMAT]: Cevap vermeden önce kısaca düşün. Düşüncelerini <think>...</think> içine yaz.";
    } 
    else if (level == "high") {
        return "\n[TALİMAT]: Bu karmaşık bir görevdir. Cevap vermeden önce DERİNLEMESİNE düşün.\n"
               "1. Problemi parçalara ayır.\n"
               "2. Her adımı analiz et.\n"
               "3. Olası hataları kontrol et.\n"
               "4. Tüm bu süreci <think>...</think> etiketleri arasına DETAYLI şekilde yaz.\n"
               "5. Sonuç olarak cevabı ver.";
    }
    return "";
}

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
      
    const char* mount_point = "/";
    const char* base_dir = "./studio-v2"; 
    if (!svr_.set_mount_point(mount_point, base_dir)) {
        spdlog::error("UI directory '{}' not found.", base_dir);
    }
    svr_.set_logger([](const httplib::Request& req, const httplib::Response& res) {
        spdlog::debug("HTTP {} {} - Status: {}", req.method, req.path, res.status);
    });

    // --- DYNAMIC UI LAYOUT ---
    svr_.Get("/v1/ui/layout", [this](const httplib::Request &, httplib::Response &res) {
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
                            {{"label", "🎨 Sanat"}, {"value", "creative"}},
                            {{"label", "🇺🇸 EN"}, {"value", "english"}}
                        }}
                    },
                    // [NEW] Reasoning Level Control
                    {
                        {"type", "segmented"},
                        {"id", "reasoning-level"},
                        {"label", "AKIL YÜRÜTME (REASONING)"},
                        {"options", {
                            {{"label", "Kapalı"}, {"value", "none"}, {"active", true}},
                            {{"label", "Hızlı"}, {"value", "low"}},
                            {{"label", "Derin"}, {"value", "high"}}
                        }}
                    },
                    {
                        {"type", "textarea"},
                        {"id", "systemPrompt"},
                        {"label", "SİSTEM TALİMATI"},
                        {"properties", { {"rows", 5} }}
                    },
                    {
                        {"type", "slider"},
                        {"id", "tempInput"},
                        {"label", "Sıcaklık"},
                        {"display_id", "tempVal"},
                        {"properties", { {"min", 0.0}, {"max", 2.0}, {"step", 0.1}, {"value", 0.7} }}
                    },
                    {
                        {"type", "slider"},
                        {"id", "tokenLimit"},
                        {"label", "Token"},
                        {"display_id", "tokenVal"},
                        {"properties", { {"min", 64}, {"max", 8192}, {"step", 64}, {"value", 1024} }}
                    }
                }},
                {"telemetry", {
                    {
                        {"type", "textarea"},
                        {"id", "ragInput"},
                        {"label", "RAG BAĞLAMI"},
                        {"properties", {
                            {"placeholder", "Dinamik olarak oluşturulmuş RAG alanı..."},
                            {"rows", 5}
                        }}
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

            // [NEW] Reasoning Injection
            std::string reasoning_level = body.value("reasoning_effort", "none");
            std::string reasoning_prompt = get_reasoning_instruction(reasoning_level);

            if (body.contains("messages") && body["messages"].is_array()) {
                const auto& messages = body["messages"];
                bool first_user_message_found = false;
                for (size_t i = 0; i < messages.size(); ++i) {
                    const auto& msg = messages[i];
                    std::string role = msg.value("role", "");
                    std::string content = msg.value("content", "");
                    
                    if (role == "system") {
                         std::string final_system = content;
                         if (!reasoning_prompt.empty()) {
                             final_system += reasoning_prompt;
                         }
                         
                         if (grpc_request.system_prompt().empty()) {
                            grpc_request.set_system_prompt(final_system);
                         } else {
                            if (!grpc_request.has_rag_context()) {
                                grpc_request.set_rag_context(final_system);
                            }
                         }
                    } else if (role == "user" && !first_user_message_found) {
                        grpc_request.set_user_prompt(content);
                        first_user_message_found = true;
                    }
                    else {
                        auto* turn = grpc_request.add_history();
                        turn->set_role(role);
                        turn->set_content(content);
                    }
                }
                
                // Eğer system prompt boşsa ama reasoning isteniyorsa
                if (grpc_request.system_prompt().empty() && !reasoning_prompt.empty()) {
                    grpc_request.set_system_prompt("Sen yardımsever bir asistansın." + reasoning_prompt);
                }
            }
            auto* params = grpc_request.mutable_params();
            if (body.contains("max_tokens")) params->set_max_new_tokens(body["max_tokens"]);
            if (body.contains("temperature")) params->set_temperature(body["temperature"]);

            bool stream = body.value("stream", false);
            auto batched_request = std::make_shared<BatchedRequest>();
            batched_request->request = grpc_request;

            if (body.contains("response_format") && body["response_format"].value("type", "") == "json_object") {
                 batched_request->grammar = R"(root ::= object; value ::= object | array | string | number | ("true" | "false" | "null") ws; object ::= "{" ws (string ":" ws value ("," ws string ":" ws value)*)? "}" ws; array ::= "[" ws (value ("," ws value)*)? "]" ws; string ::= "\"" ([^"\\\x7F\x00-\x1F] | "\\" (["\\/bfnrt] | "u" [0-9a-fA-F]{4}))* "\"" ws; number ::= ("-"? ([0-9] | [1-9] [0-9]*)) ("." [0-9]+)? ([eE] [-+]? [0-9]+)? ws; ws ::= [ \t\n\r]*)";
            } else if (body.contains("grammar")) {
                batched_request->grammar = body["grammar"].get<std::string>();
            }

            auto completion_future = engine_->get_batcher()->add_request(batched_request);

            if (stream) {
                res.set_chunked_content_provider("text/event-stream",
                    [batched_request, pending_data = std::string("")](size_t, httplib::DataSink &sink) mutable {
                        batched_request->should_stop_callback = [&sink]() { return !sink.is_writable(); };
                        
                        while (!batched_request->is_finished || !batched_request->token_queue.empty()) {
                            std::string token;
                            if (batched_request->token_queue.wait_and_pop(token, 50)) {
                                
                                std::string clean_token = sanitize_token(token);
                                if (clean_token.empty()) continue;

                                pending_data += clean_token;

                                if (has_incomplete_utf8_suffix(pending_data)) {
                                    continue;
                                }

                                json chunk;
                                chunk["id"] = "chatcmpl-" + std::to_string(std::time(nullptr));
                                chunk["object"] = "chat.completion.chunk";
                                chunk["created"] = std::time(nullptr);
                                chunk["model"] = "sentiric-llm";
                                chunk["choices"][0]["index"] = 0;
                                chunk["choices"][0]["delta"]["content"] = pending_data;
                                
                                std::string data = "data: " + chunk.dump() + "\n\n";
                                if (!sink.write(data.c_str(), data.length())) return false;
                                
                                pending_data.clear();
                            }
                        }
                        
                        if (!pending_data.empty()) {
                             json chunk;
                             chunk["id"] = "chatcmpl-" + std::to_string(std::time(nullptr));
                             chunk["object"] = "chat.completion.chunk";
                             chunk["created"] = std::time(nullptr);
                             chunk["model"] = "sentiric-llm";
                             chunk["choices"][0]["index"] = 0;
                             chunk["choices"][0]["delta"]["content"] = pending_data;
                             std::string data = "data: " + chunk.dump() + "\n\n";
                             sink.write(data.c_str(), data.length());
                        }

                        sink.write("data: [DONE]\n\n", 12);
                        sink.done();
                        return true;
                    });
            } else {
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
}

void HttpServer::run() {
    spdlog::info("📊 HTTP sunucusu (Health + Studio) {}:{} adresinde dinleniyor.", host_, port_);
    svr_.listen(host_.c_str(), port_);
}
void HttpServer::stop() { if (svr_.is_running()) svr_.stop(); }
void run_http_server_thread(std::shared_ptr<HttpServer> server) { if (server) server->run(); }
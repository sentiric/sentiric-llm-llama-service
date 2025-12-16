#include "controllers/chat_controller.h"
#include "spdlog/spdlog.h"
#include <algorithm>
#include <vector>
#include <chrono>

using json = nlohmann::json;

ChatController::ChatController(std::shared_ptr<LLMEngine> engine)
    : engine_(std::move(engine)) {}

bool ChatController::has_incomplete_utf8_suffix(const std::string& str) {
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

std::string ChatController::sanitize_token(const std::string& input) {
    std::string clean = input;
    clean.erase(std::remove_if(clean.begin(), clean.end(), [](unsigned char c) {
        return (c < 32 && c != 9 && c != 10 && c != 13) || c == 127;
    }), clean.end());
    return clean;
}

std::string ChatController::get_reasoning_instruction(const std::string& level) {
    const auto& s = engine_->get_settings();
    if (level == "low") {
        return s.reasoning_prompt_low;
    } 
    else if (level == "high") {
        return s.reasoning_prompt_high;
    }
    return "";
}

sentiric::llm::v1::GenerateStreamRequest ChatController::build_grpc_request(const json& body, const std::string& reasoning_prompt) {
    sentiric::llm::v1::GenerateStreamRequest grpc_request;
    const auto& settings = engine_->get_settings();

    std::string system_prompt = settings.template_system_prompt;
    
    if (body.contains("system_prompt") && body["system_prompt"].is_string()) {
        system_prompt = body["system_prompt"].get<std::string>();
    }

    if (body.contains("messages") && body["messages"].is_array()) {
        const auto& messages = body["messages"];
        size_t total_msgs = messages.size();
        
        for (size_t i = 0; i < total_msgs; ++i) {
            const auto& msg = messages[i];
            std::string role = msg.value("role", "");
            std::string content = msg.value("content", "");

            if (role == "system") {
                system_prompt = content;
            } else {
                if (i == total_msgs - 1 && role == "user") {
                    grpc_request.set_user_prompt(content);
                } else {
                    auto* turn = grpc_request.add_history();
                    turn->set_role(role);
                    turn->set_content(content);
                }
            }
        }
    }

    if (body.contains("rag_context") && body["rag_context"].is_string()) {
        grpc_request.set_rag_context(body["rag_context"]);
    }

    // [YENİ] LoRA adaptörünü ekle
    if (body.contains("lora_adapter") && body["lora_adapter"].is_string()) {
        grpc_request.set_lora_adapter_id(body["lora_adapter"]);
    }

    if (!reasoning_prompt.empty()) {
        system_prompt += reasoning_prompt;
    }

    if (!system_prompt.empty()) {
        grpc_request.set_system_prompt(system_prompt);
    }

    auto* params = grpc_request.mutable_params();
    if (body.contains("max_tokens")) params->set_max_new_tokens(body["max_tokens"]);
    if (body.contains("temperature")) params->set_temperature(body["temperature"]);
    if (body.contains("top_p")) params->set_top_p(body["top_p"]);
    if (body.contains("top_k")) params->set_top_k(body["top_k"]);

    return grpc_request;
}

void ChatController::handle_streaming_response(std::shared_ptr<BatchedRequest> batched_request, httplib::Response& res) {
    res.set_chunked_content_provider("text/event-stream",
        [this, batched_request, pending_data = std::string("")](size_t, httplib::DataSink &sink) mutable {
            batched_request->should_stop_callback = [&sink]() { return !sink.is_writable(); };
            
            while (!batched_request->is_finished || !batched_request->token_queue.empty()) {
                std::string token;
                if (batched_request->token_queue.wait_and_pop(token, 50)) {
                    
                    if (!batched_request->first_token_emitted.exchange(true)) {
                        auto now = std::chrono::steady_clock::now();
                        std::chrono::duration<double, std::milli> ttft = now - batched_request->creation_time;
                        batched_request->ttft_ms = ttft.count();
                        spdlog::debug("[HTTP] ⚡ TTFT: {:.2f} ms", batched_request->ttft_ms.load());
                    }

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
                    chunk["model"] = engine_->get_settings().model_id;
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
                 chunk["model"] = engine_->get_settings().model_id;
                 chunk["choices"][0]["index"] = 0;
                 chunk["choices"][0]["delta"]["content"] = pending_data;
                 std::string data = "data: " + chunk.dump() + "\n\n";
                 sink.write(data.c_str(), data.length());
            }

            sink.write("data: [DONE]\n\n", 12);
            sink.done();
            
            spdlog::info("[HTTP] Stream Complete. Tokens: {}/{}, TTFT: {:.2f}ms", 
                batched_request->prompt_tokens, batched_request->completion_tokens, batched_request->ttft_ms.load());
            
            return true;
        });
}

void ChatController::handle_unary_response(std::shared_ptr<BatchedRequest> batched_request, std::future<void>& completion_future, httplib::Response& res) {
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
    response_json["model"] = engine_->get_settings().model_id;
    response_json["choices"][0]["index"] = 0;
    response_json["choices"][0]["message"]["role"] = "assistant";
    response_json["choices"][0]["message"]["content"] = full_response;
    response_json["choices"][0]["finish_reason"] = batched_request->finish_reason;
    response_json["usage"]["prompt_tokens"] = batched_request->prompt_tokens;
    response_json["usage"]["completion_tokens"] = batched_request->completion_tokens;
    response_json["usage"]["total_tokens"] = batched_request->prompt_tokens + batched_request->completion_tokens;
    res.set_content(response_json.dump(), "application/json");
}

void ChatController::handle_chat_completions(const httplib::Request &req, httplib::Response &res) {
    res.set_header("Access-Control-Allow-Origin", "*");
    
    try {
        if (!engine_->is_model_loaded()) {
            res.status = 503; 
            res.set_content(json({{"error", {{"message", "Model is loading or not ready"}, {"type", "server_error"}}}}).dump(), "application/json"); 
            return;
        }

        json body = json::parse(req.body);
        std::string reasoning_level = body.value("reasoning_effort", "none");
        std::string reasoning_prompt = get_reasoning_instruction(reasoning_level);

        auto grpc_request = build_grpc_request(body, reasoning_prompt);
        bool stream = body.value("stream", false);

        auto batched_request = std::make_shared<BatchedRequest>();
        batched_request->request = grpc_request;

        if (body.contains("response_format") && body["response_format"].value("type", "") == "json_object") {
             batched_request->grammar = R"(
root   ::= object
value  ::= object | array | string | number | ("true" | "false" | "null")
object ::= "{" ws ( members )? "}"
members ::= pair ( "," ws pair )*
pair   ::= string ":" ws value
array  ::= "[" ws ( elements )? "]"
elements ::= value ( "," ws value )*
string ::= "\"" ([^"\\] | "\\" .)* "\"" ws
number ::= ("-"? ([0-9] | [1-9] [0-9]*)) ("." [0-9]+)? ([eE] [-+]? [0-9]+)? ws
ws     ::= [ \t\n\r]*
)";
        } else if (body.contains("grammar")) {
            batched_request->grammar = body["grammar"].get<std::string>();
        }

        auto completion_future = engine_->get_batcher()->add_request(batched_request);

        if (stream) {
            handle_streaming_response(batched_request, res);
        } else {
            handle_unary_response(batched_request, completion_future, res);
        }
    } catch (const std::exception& e) {
        spdlog::error("HTTP handler error: {}", e.what());
        res.status = 400;
        res.set_content(json({{"error", {{"message", e.what()}, {"type", "invalid_request_error"}}}}).dump(), "application/json");
    }
}
#pragma once

#include "llm_engine.h"
#include <memory>
#include <string>
#include <thread>
#include "httplib.h"

class HttpServer {
public:
HttpServer(std::shared_ptr<LLMEngine> engine, const std::string& host, int port);
void stop();
void run();

private:
httplib::Server svr_;
std::shared_ptr<LLMEngine> engine_;
std::string host_;
int port_;
std::thread server_thread_;
};

// Sunucuyu başlatan ve thread'i yöneten fonksiyon
void run_http_server_thread(std::shared_ptr<HttpServer> server);
#pragma once

#include "llm_engine.h"
#include <memory>
#include <string>

// HTTP sağlık kontrol sunucusunu başlatan fonksiyon
void run_http_server(std::shared_ptr<LLMEngine> engine, const std::string& host, int port);
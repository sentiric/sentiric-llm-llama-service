#pragma once

#include "llm_engine.h"
#include <memory>
#include <string>
#include <thread>
#include "httplib.h"
#include <prometheus/registry.h>
#include <prometheus/collectable.h>
#include <prometheus/counter.h>
#include <prometheus/histogram.h>
#include <prometheus/gauge.h>

// Metrik ailelerini tutacak ve uygulama genelinde taşınacak bir yapı.
struct AppMetrics {
    prometheus::Counter& requests_total;
    prometheus::Histogram& request_latency;
    prometheus::Counter& tokens_generated_total;
    prometheus::Gauge& active_contexts;
};

// Metrik sunucusu için ayrı bir sınıf.
class MetricsServer {
public:
    MetricsServer(const std::string& host, int port, prometheus::Registry& registry);
    void run();
    void stop();

private:
    httplib::Server svr_;
    std::string host_;
    int port_;
    prometheus::Registry& registry_;
};

void run_metrics_server_thread(std::shared_ptr<MetricsServer> server);

// Geliştirme Stüdyosu'nu sunan ana HTTP sunucusu.
class HttpServer {
public:
    HttpServer(std::shared_ptr<LLMEngine> engine, const std::string& host, int port);
    void run();
    void stop();

private:
    httplib::Server svr_;
    std::shared_ptr<LLMEngine> engine_;
    std::string host_;
    int port_;
};

void run_http_server_thread(std::shared_ptr<HttpServer> server);
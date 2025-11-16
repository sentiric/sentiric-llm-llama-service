# 🧠 Sentiric LLM Llama Service

**Production-ready** C++ local LLM inference service. Built for high performance, true concurrency, and reliability.

[![CI - Build and Push Docker Image](https://github.com/sentiric/sentiric-llm-llama-service/actions/workflows/build-and-push.yml/badge.svg)](https://github.com/sentiric/sentiric-llm-llama-service/actions/workflows/build-and-push.yml)

## 🚀 Features

-   ✅ **True Concurrency**: High-performance, concurrent request handling via a `LlamaContextPool`.
-   ✅ **Advanced Memory Management**: Supports near-infinite context sizes even on limited hardware via "Context Shifting".
-   ✅ **Performance Optimized**: Leverages `mmap`, KV Cache GPU offloading, and NUMA awareness for maximum throughput.
-   ✅ **gRPC Streaming**: Real-time, token-by-token generation for interactive applications.
-   ✅ **Built-in Observability**: Provides a `/health` endpoint for health checks and a `/metrics` endpoint for detailed Prometheus metrics.
-   ✅ **Modern C++ & llama.cpp**: Built on C++17 with a stable, recent version of the `llama.cpp` backend.
-   ✅ **Dockerized & Optimized**: Deploys as a minimal and efficient multi-stage Docker container for both CPU and NVIDIA GPU.

---

## 📚 Documentation

-   **[Quick Start & Deployment](./docs/guides/02_DEPLOYMENT.md)**
-   **[Configuration Guide](./docs/guides/03_CONFIGURATION.md)**
-   **[System Architecture](./docs/architecture/SYSTEM_ARCHITECTURE.md)**
-   **[Performance & Memory Tuning](./docs/architecture/PERFORMANCE_AND_MEMORY.md)** (NEW)

---

## 🛠️ Quick Start

### Prerequisites

-   Docker & Docker Compose
-   4GB+ RAM
-   Git

### Running the Service (CPU)

#### For Development (Building from source)
```bash
# Clone the repository
git clone https://github.com/sentiric/sentiric-llm-llama-service.git
cd sentiric-llm-llama-service

# Build and run the service (uses docker-compose.override.yml automatically)
docker compose up --build -d
```

#### For Production (Pulling pre-built image)
```bash
# Pull the latest CPU image and run
docker compose -f docker-compose.yml -f docker-compose.cpu.yml up -d
```

### Running the Service (GPU)

For GPU instructions and other advanced scenarios, please refer to the **[Deployment Guide](./docs/guides/02_DEPLOYMENT.md)**.

### Verifying the Service

# Check service health (wait up to 1-2 minutes for the model to load)

```bash
curl http://localhost:16070/health
# Expected output: {"status":"healthy","model_ready":true,"engine":"llama.cpp"}
```

# Test with the CLI tool using a RAG scenario
```bash
./run_request.sh examples/hospitality_service_context.txt "Ahmet Bey'in rezervasyon durumu nedir?"
```

---

## 📊 Monitoring & Metrics

The service provides a Prometheus-compatible metrics endpoint at `http://localhost:16072/metrics`. Key metrics include:

-   `llm_requests_total`: Total number of gRPC requests received.
-   `llm_request_latency_seconds`: A histogram of request processing times.
-   `llm_tokens_generated_total`: Total number of tokens generated.
-   `llm_active_contexts`: The number of `llama_context` instances currently in use.

This allows for detailed monitoring of the service's performance, load, and resource utilization.

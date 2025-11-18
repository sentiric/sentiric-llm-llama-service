# 🧠 Sentiric LLM Llama Service

**Production-ready** C++ local LLM inference service. Built for high performance, true concurrency, and reliability.

[![CI - Build and Push Docker Image](https://github.com/sentiric/sentiric-llm-llama-service/actions/workflows/build-and-push.yml/badge.svg)](https://github.com/sentiric/sentiric-llm-llama-service/actions/workflows/build-and-push.yml)

## 🚀 Features

-   ✅ **True Concurrency**: High-performance, concurrent request handling via a thread-safe `LlamaContextPool`.
-   ✅ **Stateful Conversations**: Remembers previous turns in a dialogue for context-aware responses.
-   ✅ **Retrieval-Augmented Generation (RAG)**: Dynamically uses provided context to answer questions accurately.
-   ✅ **Advanced Memory Management**: Supports near-infinite context sizes even on limited hardware via "Context Shifting".
-   ✅ **Performance Optimized**: Leverages `mmap`, KV Cache GPU offloading, and NUMA awareness for maximum throughput.
-   ✅ **gRPC Streaming**: Real-time, token-by-token generation for interactive applications.
-   ✅ **Built-in Observability**: Provides a `/health` endpoint and a `/metrics` endpoint for detailed Prometheus metrics.
-   ✅ **Modern C++ & llama.cpp**: Built on C++17 with a stable, version-pinned `llama.cpp` backend.
-   ✅ **Dockerized & Optimized**: Deploys as a minimal and efficient multi-stage Docker container for both CPU and NVIDIA GPU.

---

## 📚 Documentation

-   **[Quick Start & Deployment](./docs/guides/02_DEPLOYMENT.md)**
-   **[Configuration Guide](./docs/guides/03_CONFIGURATION.md)**
-   **[System Architecture](./docs/architecture/SYSTEM_ARCHITECTURE.md)**
-   **[Performance & Memory Tuning](./docs/architecture/PERFORMANCE_AND_MEMORY.md)**

---

## 🛠️ Quick Start

### Prerequisites

-   Docker & Docker Compose
-   Git
-   4GB+ RAM (8GB+ recommended)

### Running the Service (CPU)

```bash
# Clone the repository
git clone https://github.com/sentiric/sentiric-llm-llama-service.git
cd sentiric-llm-llama-service

# Build and run the service for development (uses docker-compose.override.yml)
docker compose up --build -d

# Or pull the pre-built image for production
# docker compose -f docker-compose.yml -f docker-compose.cpu.yml up -d
```

### Running the Service (GPU)

For GPU instructions and other advanced scenarios, please refer to the **[Deployment Guide](./docs/guides/02_DEPLOYMENT.md)**.

### Verifying the Service

Wait up to 1-2 minutes for the model to download and load on the first run.

```bash
# Check service health
curl http://localhost:16070/health
# Expected output: {"status":"healthy","model_ready":true,"engine":"llama.cpp"}

# Test with the CLI tool using a RAG scenario
docker compose run --rm llm-cli llm_cli generate "Ahmet Bey'in rezervasyon durumu nedir?" \
  --rag-context "$(cat examples/hospitality_service_context.txt)"
```

---

## 📊 Monitoring & Metrics

The service provides a Prometheus-compatible metrics endpoint at `http://localhost:16072/metrics`. Key metrics include:

-   `llm_requests_total`: Total number of gRPC requests received.
-   `llm_request_latency_seconds`: A histogram of request processing times.
-   `llm_tokens_generated_total`: Total number of tokens generated.
-   `llm_active_contexts`: The number of `llama_context` instances currently in use.

This allows for detailed monitoring of the service's performance, load, and resource utilization.
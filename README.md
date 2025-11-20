# 🧠 Sentiric LLM Llama Service

**Production-ready**, high-performance C++ local LLM inference engine. Powered by `llama.cpp`, optimized for NVIDIA GPUs, and featuring the new **Sentiric Omni-Studio** interface.

[![CI - Build and Push Docker Image](https://github.com/sentiric/sentiric-llm-llama-service/actions/workflows/build-and-push.yml/badge.svg)](https://github.com/sentiric/sentiric-llm-llama-service/actions/workflows/build-and-push.yml)

## 🚀 Key Features

-   ✅ **True Concurrency**: Handles multiple requests simultaneously via a thread-safe `LlamaContextPool`.
-   ✅ **Sentiric Omni-Studio**: A professional, mobile-first web UI with **Hands-Free Voice Mode**, RAG injection, and live telemetry.
-   ✅ **Smart Caching**: Eliminates redundant processing with intelligent KV cache management (Zero-Decode logic implemented).
-   ✅ **Robust RAG Support**: Supports huge context injection with auto-truncation protection to prevent OOM crashes.
-   ✅ **Performance Optimized**: 
    -   **GPU Offloading**: Full VRAM utilization for both weights and KV cache.
    -   **Fast Warm-up**: CUDA JIT pre-heating for instant first response.
    -   **No-MMAP**: Option to force full RAM loading for stability.
-   ✅ **Structured Output**: Supports GBNF grammar constraints (JSON mode etc.).
-   ✅ **gRPC & HTTP**: Dual-stack API for high-performance internal comms and easy web integration.

---

## 📸 Sentiric Omni-Studio

The service includes a built-in development studio available at `http://localhost:16070`.

-   **Hands-Free Mode:** Double-click the mic button to start a continuous voice conversation loop.
-   **RAG Drag & Drop:** Upload files to instantly inject them into the context window.
-   **Mobile Optimized:** Responsive design that works perfectly on phones and tablets.

---

## 🛠️ Quick Start

### Prerequisites

-   Docker & Docker Compose
-   NVIDIA GPU (Recommended) or High-Performance CPU

### Running with GPU (Recommended)

```bash
# Clone the repository
git clone https://github.com/sentiric/sentiric-llm-llama-service.git
cd sentiric-llm-llama-service

# Start the service (Downloads model automatically)
docker compose -f docker-compose.yml -f docker-compose.gpu.yml -f docker-compose.gpu.override.yml up -d
```

### Running with CPU

```bash
docker compose up -d
```

---

## 📊 Monitoring

-   **Health Check:** `http://localhost:16070/health`
-   **Prometheus Metrics:** `http://localhost:16072/metrics`

Key metrics include `llm_active_contexts`, `llm_tokens_generated_total`, and `llm_request_latency_seconds`.

## 📚 Documentation

-   **[Deployment Guide](./docs/guides/02_DEPLOYMENT.md)**
-   **[Configuration Reference](./docs/guides/03_CONFIGURATION.md)**
-   **[Architecture Details](./docs/architecture/SYSTEM_ARCHITECTURE.md)**

---

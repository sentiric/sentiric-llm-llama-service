# 🧠 Sentiric LLM Llama Service

**Production-Ready**, high-performance C++ local LLM inference engine. Powered by `llama.cpp`, optimized for NVIDIA GPUs, and featuring the new **Sentiric Omni-Studio** interface.

[![CI - Build and Push Docker Image](https://github.com/sentiric/sentiric-llm-llama-service/actions/workflows/build-and-push.yml/badge.svg)](https://github.com/sentiric/sentiric-llm-llama-service/actions/workflows/build-and-push.yml)

## 🚀 Status: PRODUCTION READY

This service has passed rigorous testing for concurrency, memory stability, and API compliance. It is ready for deployment in core ecosystem environments.

-   ✅ **True Concurrency**: Handles multiple requests simultaneously via `LlamaContextPool`.
-   ✅ **Deep Health Checks**: Exposes capacity and model status for load balancers.
-   ✅ **Observability**: Structured JSON logs (prod) and Prometheus metrics.
-   ✅ **Security**: Full mTLS support for gRPC communication.
-   ✅ **Standalone Mode**: Can be developed and tested independently using the embedded Studio.

---

## 📸 Sentiric Omni-Studio

The service includes a built-in development studio available at `http://localhost:16070`.

-   **Hands-Free Mode:** Continuous voice conversation loop.
-   **RAG Drag & Drop:** Instant context injection.
-   **Mobile Optimized:** Responsive design.

---

## 🛠️ Quick Start (Standalone)

### Prerequisites
-   Docker & Docker Compose
-   NVIDIA GPU (Recommended)

### Running in Isolation

```bash
# 1. Clone
git clone https://github.com/sentiric/sentiric-llm-llama-service.git
cd sentiric-llm-llama-service

# 2. Start (Auto-downloads model & certificates for dev)
make up-gpu
# OR for CPU:
# make up-cpu
```

**Access Points:**
-   **Studio UI:** `http://localhost:16070`
-   **Metrics:** `http://localhost:16072/metrics`
-   **gRPC:** `localhost:16071`

---

## 📊 Monitoring & Metrics

Key Prometheus metrics exposed at `:16072/metrics`:

| Metric | Description |
| :--- | :--- |
| `llm_active_contexts` | Number of currently busy contexts in the pool. |
| `llm_requests_total` | Total number of gRPC requests processed. |
| `llm_tokens_generated_total` | Total tokens generated across all requests. |
| `llm_request_latency_seconds` | Histogram of request processing times. |

---

## 📚 Documentation

-   **[Critical API Bindings](./docs/KB/04_LLAMA_CPP_API_BINDING.md)**
-   **[Deployment Guide](./docs/guides/02_DEPLOYMENT.md)**
-   **[Configuration Reference](./docs/guides/03_CONFIGURATION.md)**
-   **[Architecture Details](./docs/architecture/SYSTEM_ARCHITECTURE.md)**


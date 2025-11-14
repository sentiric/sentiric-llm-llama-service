# 🧠 Sentiric LLM Llama Service

**Production-ready** C++ local LLM inference service. Built for high performance, true concurrency, and reliability.

[![CI - Build and Push Docker Image](https://github.com/sentiric/sentiric-llm-llama-service/actions/workflows/build-and-push.yml/badge.svg)](https://github.com/sentiric/sentiric-llm-llama-service/actions/workflows/build-and-push.yml)

## 🚀 Features

-   ✅ **True Concurrency**: High-performance, concurrent request handling via a `LlamaContextPool`. The service can process multiple requests in parallel, maximizing hardware utilization.
-   ✅ **gRPC Streaming**: Real-time, token-by-token generation for interactive applications.
-   ✅ **HTTP Health Check**: `/health` endpoint for robust monitoring and orchestration.
-   ✅ **Modern C++ & llama.cpp**: Built on a modern C++17 stack using a stable, recent version of the `llama.cpp` backend.
-   ✅ **Dockerized & Optimized**: Deploys as a minimal and efficient multi-stage Docker container for both CPU and NVIDIA GPU.
-   ✅ **CLI Tool**: Includes `llm_cli` for testing, benchmarking, and health checks.

---

## 📚 Documentation

All detailed project documentation is located in the `/docs` directory.

-   **[Quick Start & Deployment](./docs/guides/02_DEPLOYMENT.md)**
-   **[Developer Guide](./docs/guides/01_DEVELOPMENT.md)**
-   **[System Architecture](./docs/architecture/SYSTEM_ARCHITECTURE.md)**
-   **[API Specification](./docs/API_SPECIFICATION.md)**
-   **[Knowledge Base & Troubleshooting](./docs/kb/03_SOLVED_ISSUES.md)**
-   **[Development Tasks](./docs/TASKS.md)**

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

```bash
# Check service health (wait up to 1-2 minutes for the model to load)
curl http://localhost:16070/health
# Expected output: {"engine":"llama.cpp","model_ready":true,"status":"healthy"}

# Test with the CLI tool
docker compose run --rm llm-cli llm_cli generate "mTLS bağlantısı bu sefer gerçekten başarılı mı?"
```
---

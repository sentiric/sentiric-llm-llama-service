# 🧠 Sentiric LLM Llama Service

**Production-ready** C++ local LLM inference service. Built for high performance, concurrency, and reliability.

[![CI - Build and Push Docker Image](https://github.com/sentiric/sentiric-llm-llama-service/actions/workflows/build-and-push.yml/badge.svg)](https://github.com/sentiric/sentiric-llm-llama-service/actions/workflows/build-and-push.yml)

## 🚀 Features

-   ✅ **High Performance**: Concurrent request handling via a context pool.
-   ✅ **gRPC Streaming**: Real-time, token-by-token generation.
-   ✅ **HTTP Health Check**: `/health` endpoint for robust monitoring.
-   ✅ **Modern C++ & llama.cpp**: Built on a modern C++17 stack using the latest `llama.cpp` backend.
-   ✅ **Dockerized**: Deploys as a minimal and efficient Docker container.
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

### Running the Service

```bash
# Clone the repository
git clone https://github.com/sentiric/sentiric-llm-llama-service.git
cd sentiric-llm-llama-service

# Build and run the service
docker compose up --build -d

# Check service health (wait up to 1-2 minutes for the model to load)
curl http://localhost:16070/health
# Expected output: {"engine":"llama.cpp","model_ready":true,"status":"healthy"}

# Test with the CLI tool
docker compose exec llm-llama-service llm_cli generate "Hello, what is your name?"
```

---

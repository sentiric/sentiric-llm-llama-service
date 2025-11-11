# 🧠 Sentiric LLM Llama Service

**Production-ready** C++ local LLM inference service. Built for high performance, concurrency, and reliability using a modular, layered Docker architecture.

[![CI/CD Status](https://github.com/sentiric/sentiric-llm-llama-service/actions/workflows/build.yml/badge.svg)](https://github.com/sentiric/sentiric-llm-llama-service/actions)

## 🚀 Features

-   ✅ **High Performance**: Concurrent request handling via a context pool, maximizing CPU usage.
-   ✅ **gRPC Streaming**: Real-time, token-by-token generation for interactive applications.
-   ✅ **HTTP Health Check**: `/health` endpoint for robust monitoring and orchestration (`200 OK` or `503 Service Unavailable`).
-   ✅ **Layered Docker Builds**: Fast and efficient CI/CD using pre-built base images for `vcpkg` and `llama.cpp`.
-   ✅ **Command-Line Interface (CLI)**: A powerful `llm_cli` tool for testing, benchmarking, and health checks.
-   ✅ **Reliable & Portable**: Deploys as a minimal Docker container with robust error handling and graceful shutdown.

## 📦 Technical Architecture

The service is built on a layered Docker image foundation to ensure fast, reproducible builds:

1.  `ghcr.io/sentiric/vcpkg-base`: Contains build tools and the vcpkg package manager.
2.  `ghcr.io/sentiric/sentiric-llama-cpp`: Compiles and installs `llama.cpp` as a system-wide shared library (`libllama.so`) along with its CMake package configuration.
3.  `sentiric-llm-llama-service`: The final application layer, which links against `libllama.so`.

For a detailed diagram and concurrency model, see [ARCHITECTURE.md](ARCHITECTURE.md).

### Port Configuration

-   **HTTP Health**: `16060`
-   **gRPC Service**: `16061`

## 🛠️ Quick Start

### Prerequisites

-   Docker & Docker Compose
-   4GB+ RAM
-   5GB+ Disk Space

### Running the Service

```bash
# 1. Clone the repository
git clone https://github.com/sentiric/sentiric-llm-llama-service.git
cd sentiric-llm-llama-service

# 2. Download the required model
./models/download.sh

# 3. Build and run the service
docker compose up --build -d

# 4. Check service health (wait up to 1-2 minutes for the model to load)
curl http://localhost:16060/health
# Expected output: {"engine":"llama.cpp","model_ready":true,"status":"healthy"}

# 5. Test with the CLI tool
docker compose exec llm-llama-service llm_cli generate "Hello, what is your name?"
```

## 🔧 Developer Guide

### Using the Command-Line Interface (`llm_cli`)

The service container includes `llm_cli` for easy interaction and diagnostics.

```bash
# Get help and see all available commands
docker compose exec llm-llama-service llm_cli

# Run a gRPC stream generation request
docker compose exec llm-llama-service llm_cli generate "Ürün iade etmek istiyorum"

# Get a detailed health report for the service
docker compose exec llm-llama-service llm_cli health

# Run a simple performance benchmark with 20 sequential requests
docker compose exec llm-llama-service llm_cli benchmark --iterations 20
```

## 🐛 Troubleshooting

#### Build Fails with `target "llama::llama" not found`

-   **Cause**: The `ghcr.io/sentiric/sentiric-llama-cpp` base image is outdated or was built incorrectly.
-   **Solution**: Ensure you have the latest version of the base image (`docker pull ghcr.io/sentiric/sentiric-llama-cpp:latest`) or rebuild it using its own repository, which correctly installs the CMake package files.

#### Service is unhealthy (`503 Service Unavailable`)

-   **Cause**: This is the correct behavior if the model file specified by `LLM_MODEL_PATH` cannot be loaded.
-   **Solution**:
    1.  Verify the model file exists: `ls -l models/`.
    2.  Check container logs for "Model loading failed" errors: `docker logs llm-llama-service`.
    3.  Ensure you have enough free memory on your host machine.

---

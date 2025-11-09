# sentiric-llm-llama-service/Dockerfile
# --- Derleme Aşaması ---
FROM ghcr.io/sentiric/sentiric-llama-cpp:latest AS builder

WORKDIR /app

# ADIM 1: Bağımlılıkları kur (vcpkg zaten hazır)
COPY vcpkg.json .
RUN /opt/vcpkg/vcpkg install --triplet x64-linux

# ADIM 2: Kodları kopyala ve build et (llama.cpp sistemde hazır)
COPY . .

# Build işlemi - llama.cpp artık sistem kütüphanesi
RUN cmake -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=OFF \
    -DCMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake

RUN cmake --build build --target all -j $(nproc)

# --- Çalışma Aşaması ---
FROM ubuntu:22.04

# Runtime dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates libgomp1 && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Sadece executable'ları kopyala
COPY --from=builder /app/build/llm_service /usr/local/bin/llm_service
COPY --from=builder /app/build/grpc_test_client /usr/local/bin/grpc_test_client

# Model dizinini oluştur
RUN mkdir -p /models

# Port ve environment variables
EXPOSE 16060 16061
ENV LLM_LOCAL_SERVICE_MODEL_PATH="/models/phi-3-mini.q4.gguf"

# Health check
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD curl -f http://localhost:16060/health || exit 1

CMD ["llm_service"]
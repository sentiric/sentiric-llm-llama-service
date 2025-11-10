# sentiric-llm-llama-service/Dockerfile
# --- Derleme Aşaması ---
FROM ghcr.io/sentiric/sentiric-llama-cpp:latest AS builder

WORKDIR /app

# 1. Contracts reposunu clone et
RUN git clone https://github.com/sentiric/sentiric-contracts.git && \
    mkdir -p gen/sentiric/llm/v1 && \
    cp -r sentiric-contracts/gen/cpp/sentiric/llm/v1/* gen/sentiric/llm/v1/ && \
    rm -rf sentiric-contracts

# 2. Bağımlılıkları kur
COPY vcpkg.json .
RUN /opt/vcpkg/vcpkg install --triplet x64-linux

# 3. Kodları kopyala
COPY . .

# 4. Build - BASİT
RUN cmake -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=OFF

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
COPY --from=builder /app/build/llm_cli /usr/local/bin/llm_cli

# Model dizinini oluştur
RUN mkdir -p /models

# Port ve environment variables
EXPOSE 16060 16061
ENV LLM_LOCAL_SERVICE_MODEL_PATH="/models/phi-3-mini.q4.gguf"

CMD ["llm_service"]
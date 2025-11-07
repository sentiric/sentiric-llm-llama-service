# --- Derleme Aşaması ---
FROM ubuntu:22.04 AS builder

# Gerekli paketleri kur
RUN apt-get update && apt-get install -y --no-install-recommends \
    git cmake ninja-build build-essential curl zip unzip tar \
    ca-certificates pkg-config protobuf-compiler

# vcpkg'yi kur
RUN git clone https://github.com/microsoft/vcpkg.git /opt/vcpkg
RUN /opt/vcpkg/bootstrap-vcpkg.sh -disableMetrics

WORKDIR /app

# Tüm dosyaları kopyala
COPY . .

# Bağımlılıkları kur
RUN /opt/vcpkg/vcpkg install --triplet x64-linux

# llama.cpp'yi klonla
RUN git clone --depth 1 https://github.com/ggml-org/llama.cpp.git vendor/llama.cpp

# --- MANUEL PROTO DERLEME ---
RUN mkdir -p /app/src
# protoc'un bulduğu grpc_cpp_plugin'i kullanmak için.
RUN PLUGIN_PATH=$(find /opt/vcpkg -name "grpc_cpp_plugin" -type f | head -n 1) && \
    echo "Found plugin at: ${PLUGIN_PATH}" && \
    protoc \
        -I/app/proto \
        --grpc_out=/app/src \
        --cpp_out=/app/src \
        --plugin=protoc-gen-grpc=${PLUGIN_PATH} \
        --experimental_allow_proto3_optional \
        /app/proto/local.proto

# Derleme
RUN cmake -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake

RUN cmake --build build --target llm_service -j $(nproc)

# --- Çalışma Aşaması ---
FROM ubuntu:22.04
RUN apt-get update && apt-get install -y --no-install-recommends ca-certificates && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=builder /app/build/llm_service /usr/local/bin/llm_service
RUN mkdir -p /models

EXPOSE 16060 16061

ENV LLM_LOCAL_SERVICE_MODEL_PATH="/models/phi-3-mini.q4.gguf"
ENV LOG_LEVEL="info"
ENV LLM_LOCAL_SERVICE_HTTP_PORT=16060
ENV LLM_LOCAL_SERVICE_GRPC_PORT=16061

CMD ["llm_service"]
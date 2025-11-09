# --- Derleme Aşaması ---
FROM ubuntu:22.04 AS builder

# Gerekli paketleri kur
RUN apt-get update && apt-get install -y --no-install-recommends \
    git cmake build-essential curl zip unzip tar \
    ca-certificates pkg-config ninja-build libcurl4-openssl-dev && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

# vcpkg'yi kur
RUN git clone https://github.com/microsoft/vcpkg.git /opt/vcpkg
RUN /opt/vcpkg/bootstrap-vcpkg.sh -disableMetrics

# Llama.cpp'yi build et
WORKDIR /tmp
RUN git clone https://github.com/ggerganov/llama.cpp.git \
    && cd llama.cpp \
    && mkdir build && cd build \
    && cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DLLAMA_BUILD_SERVER=OFF \
        -DGGML_CUDA=OFF \
        -DLLAMA_CLBLAST=ON \
        -DBUILD_SHARED_LIBS=OFF \
    && cmake --build . --config Release -j$(nproc) \
    && cmake --install .

WORKDIR /app

# ADIM 1: Bağımlılıkları kur
COPY vcpkg.json .
RUN /opt/vcpkg/vcpkg install --triplet x64-linux

# ADIM 2: Kodları kopyala ve build et
COPY . .
RUN cmake -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DLLAMA_STATIC=ON \
    -DBUILD_SHARED_LIBS=OFF \
    -DCMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake
RUN cmake --build build --target all -j $(nproc)

# --- Çalışma Aşaması ---
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates libgomp1 && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=builder /app/build/llm_service /usr/local/bin/llm_service
COPY --from=builder /app/build/grpc_test_client /usr/local/bin/grpc_test_client

RUN mkdir -p /models
EXPOSE 16060 16061
ENV LLM_LOCAL_SERVICE_MODEL_PATH="/models/phi-3-mini.q4.gguf"

CMD ["llm_service"]
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

WORKDIR /app

# ADIM 1: Önce sadece vcpkg bağımlılık tanım dosyasını kopyala.
COPY vcpkg.json .

# ADIM 2: Kaynak kodun geri kalanını kopyalamadan ÖNCE bağımlılıkları kur.
RUN /opt/vcpkg/vcpkg install --triplet x64-linux

# ADIM 3: Projenin TAMAMINI kopyala
COPY . .

# ADIM 4: llama.cpp'yi proje içinde build et
RUN git clone https://github.com/azmisahin-forks/llama.cpp.git llama.cpp
RUN cd llama.cpp && git checkout 0750a59903688746883b0ecb24ac5ceed68edbf1

# Derleme - STATIC BUILD ZORUNLU
RUN cmake -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DLLAMA_STATIC=ON \
    -DBUILD_SHARED_LIBS=OFF \
    -DCMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake
RUN cmake --build build --target all -j $(nproc)

# --- Çalışma Aşaması ---
FROM ubuntu:22.04

# KRİTİK: OpenMP kütüphanesini yükle
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates libgomp1 && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Sadece executable'ları kopyala
COPY --from=builder /app/build/llm_service /usr/local/bin/llm_service
COPY --from=builder /app/build/grpc_test_client /usr/local/bin/grpc_test_client

RUN mkdir -p /models

EXPOSE 16060 16061
ENV LLM_LOCAL_SERVICE_MODEL_PATH="/models/phi-3-mini.q4.gguf"

CMD ["llm_service"]
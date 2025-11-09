# --- Derleme Aşaması ---
FROM ubuntu:22.04 AS builder

# Gerekli paketleri kur
RUN apt-get update && apt-get install -y --no-install-recommends \
    git cmake build-essential curl zip unzip tar \
    ca-certificates pkg-config ninja-build && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

# vcpkg'yi kur
RUN git clone https://github.com/microsoft/vcpkg.git /opt/vcpkg
RUN /opt/vcpkg/bootstrap-vcpkg.sh -disableMetrics

WORKDIR /app

# DÜZELTME 1: Önce sadece submodule'ü çekmek için gerekli dosyaları kopyala
COPY .gitmodules .
COPY .git .git

# DÜZELTME 2: Submodule'leri (llama.cpp) erkenden yükle
RUN git submodule update --init --recursive

# DÜZELTME 3: Projenin geri kalanını kopyala
COPY . .

# vcpkg bağımlılıklarını kur (vcpkg.json dosyasını kullanır)
RUN /opt/vcpkg/vcpkg install --triplet x64-linux

# Derleme
RUN cmake -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake
RUN cmake --build build --target all -j $(nproc)

# --- Çalışma Aşaması ---
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y --no-install-recommends ca-certificates && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

WORKDIR /app
# Çalıştırılabilir dosyaları kopyala
COPY --from=builder /app/build/llm_service /usr/local/bin/llm_service
COPY --from=builder /app/build/grpc_test_client /usr/local/bin/grpc_test_client
RUN mkdir -p /models

EXPOSE 16060 16061
ENV LLM_LOCAL_SERVICE_MODEL_PATH="/models/phi-3-mini.q4.gguf"
# Diğer ENV değişkenleri...

CMD ["llm_service"]
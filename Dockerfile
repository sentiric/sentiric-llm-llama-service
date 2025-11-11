# --- Derleme Aşaması ---
FROM ubuntu:22.04 AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    git cmake build-essential curl zip unzip tar \
    pkg-config ninja-build ca-certificates python3

# vcpkg'yi kur
ARG VCPKG_VERSION=2024.05.24
RUN curl -L "https://github.com/microsoft/vcpkg/archive/refs/tags/${VCPKG_VERSION}.tar.gz" -o vcpkg.tar.gz && \
    mkdir -p /opt/vcpkg && \
    tar xzf vcpkg.tar.gz -C /opt/vcpkg --strip-components=1 && \
    rm vcpkg.tar.gz && \
    /opt/vcpkg/bootstrap-vcpkg.sh -disableMetrics

WORKDIR /app

# 1. vcpkg bağımlılıklarını kur
COPY vcpkg.json .
RUN /opt/vcpkg/vcpkg install --triplet x64-linux

# 2. llama.cpp'nin en güncel halini klonla
RUN git clone https://github.com/ggerganov/llama.cpp.git llama.cpp

# 3. Proje kaynak kodunu kopyala
COPY src ./src
COPY CMakeLists.txt .

# 4. Projeyi derle
RUN cmake -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake
RUN cmake --build build --target all -j $(nproc)

# --- Çalışma Aşaması ---
FROM ubuntu:22.04 AS runtime

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates libgomp1 && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

# Çalıştırılabilir dosyaları kopyala
COPY --from=builder /app/build/llm_service /usr/local/bin/
COPY --from=builder /app/build/llm_cli /usr/local/bin/

# Gerekli tüm paylaşılan kütüphaneleri kopyala
COPY --from=builder /app/build/bin/*.so /usr/local/lib/

# Dinamik linker önbelleğini güncelle
RUN ldconfig

RUN mkdir -p /models

EXPOSE 16070 16071
# Varsayılan model yolu için yeni standardı kullan
ENV LLM_LLAMA_SERVICE_MODEL_PATH="/models/phi-3-mini.q4.gguf"

CMD ["llm_service"]
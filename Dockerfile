# --- Derleme Aşaması ---
FROM ubuntu:22.04 AS builder

# 1. Temel bağımlılıkları kur
RUN apt-get update && apt-get install -y --no-install-recommends \
    git cmake build-essential curl zip unzip tar \
    pkg-config ninja-build ca-certificates python3

# 2. Vcpkg'yi kur
ARG VCPKG_VERSION=2024.05.24
RUN curl -L "https://github.com/microsoft/vcpkg/archive/refs/tags/${VCPKG_VERSION}.tar.gz" -o vcpkg.tar.gz && \
    mkdir -p /opt/vcpkg && \
    tar xzf vcpkg.tar.gz -C /opt/vcpkg --strip-components=1 && \
    rm vcpkg.tar.gz && \
    /opt/vcpkg/bootstrap-vcpkg.sh -disableMetrics

WORKDIR /app

# 3. Bağımlılık manifest'ini kopyala ve kur
COPY vcpkg.json .
RUN /opt/vcpkg/vcpkg install --triplet x64-linux

# 4. llama.cpp'yi klonla ve belirli bir versiyona sabitle
ARG LLAMA_CPP_VERSION=92bb442ad999a0d52df0af2730cd861012e8ac5c
RUN git clone https://github.com/ggml-org/llama.cpp.git llama.cpp && \
    cd llama.cpp && \
    git checkout ${LLAMA_CPP_VERSION}

# 5. Proje kaynak kodunu ve build script'ini kopyala
COPY src ./src
COPY CMakeLists.txt .

# 6. Projeyi derle
RUN cmake -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake \
    -DLLAMA_CURL=OFF
RUN cmake --build build --target all -j $(nproc)

# --- Çalışma Aşaması ---
FROM ubuntu:22.04 AS runtime

# curl komutu için curl paketini ve diğer temel bağımlılıkları kuruyoruz.
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates libgomp1 curl && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

COPY --from=builder /app/build/llm_service /usr/local/bin/
COPY --from=builder /app/build/llm_cli /usr/local/bin/

# vcpkg'nin kurduğu TÜM paylaşılan kütüphaneleri kopyala.
COPY --from=builder /app/vcpkg_installed/x64-linux/lib/*.so* /usr/local/lib/
# llama.cpp'nin kendi kütüphanelerini kopyala
COPY --from=builder /app/build/bin/*.so /usr/local/lib/

# Dinamik linker'ın yeni kütüphaneleri bulmasını sağla
RUN ldconfig

COPY web /app/web
COPY examples /app/examples
WORKDIR /app
    
RUN mkdir -p /models

EXPOSE 16070 16071

CMD ["llm_service"]
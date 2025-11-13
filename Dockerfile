# --- Derleme Aşaması ---
FROM ubuntu:24.04 AS builder

# 1. Temel bağımlılıkları kur (En az değişen katman)
RUN apt-get update && apt-get install -y --no-install-recommends \
    git cmake build-essential curl zip unzip tar \
    pkg-config ninja-build ca-certificates python3

# 2. Vcpkg'yi kur (Nadiren değişir)
ARG VCPKG_VERSION=2024.05.24
RUN curl -L "https://github.com/microsoft/vcpkg/archive/refs/tags/${VCPKG_VERSION}.tar.gz" -o vcpkg.tar.gz && \
    mkdir -p /opt/vcpkg && \
    tar xzf vcpkg.tar.gz -C /opt/vcpkg --strip-components=1 && \
    rm vcpkg.tar.gz && \
    /opt/vcpkg/bootstrap-vcpkg.sh -disableMetrics

WORKDIR /app

# 3. Bağımlılık manifest'ini kopyala ve kur (vcpkg.json değiştiğinde bu katman yeniden çalışır)
COPY vcpkg.json .
RUN /opt/vcpkg/vcpkg install --triplet x64-linux

# 4. llama.cpp'yi klonla ve belirli bir versiyona sabitle (BUILD STABILITY)
ARG LLAMA_CPP_VERSION=b7046
RUN git clone https://github.com/ggml-org/llama.cpp.git llama.cpp && \
    cd llama.cpp && \
    git checkout ${LLAMA_CPP_VERSION}

# 5. Proje kaynak kodunu ve build script'ini kopyala (En sık değişen katmanlar)
# BU ADIMLARDAN BİRİ DEĞİŞTİĞİNDE, AŞAĞIDAKİ TÜM ADIMLAR YENİDEN ÇALIŞIR.
COPY src ./src
COPY CMakeLists.txt .

# 6. Projeyi derle (Kod her değiştiğinde bu adım yeniden çalışacaktır)
RUN cmake -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake
RUN cmake --build build --target all -j $(nproc)

# --- Çalışma Aşaması ---
FROM ubuntu:24.04 AS runtime

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates libgomp1 curl && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

COPY --from=builder /app/build/llm_service /usr/local/bin/
COPY --from=builder /app/build/llm_cli /usr/local/bin/

COPY --from=builder /app/build/bin/*.so /usr/local/lib/
RUN ldconfig

RUN mkdir -p /models

EXPOSE 16070 16071

CMD ["llm_service"]
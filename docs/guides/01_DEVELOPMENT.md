# 👨‍💻 Geliştirici Rehberi

## Build Sistemi

### Bağımlılık Yönetimi
-   **vcpkg**: `gRPC`, `spdlog`, `httplib` gibi temel C++ bağımlılıklarını yönetir.
-   **`llama.cpp`**: Docker build sırasında `master` branch'ten klonlanır ve anlık olarak derlenir. Proje, `llama.cpp`'yi bir alt dizin olarak kullanır ve ona karşı dinamik olarak linklenir.
-   **Protobuf**: `CMake`'in `FetchContent` modülü, `sentiric-contracts` reposunu derleme anında çeker ve `protoc` ile gRPC/Protobuf kodlarını üretir.

### CMake Yapılandırması
`CMakeLists.txt` dosyası, `vcpkg` toolchain'i kullanarak bağımlılıkları bulur. `llama.cpp` projesini `add_subdirectory` ile dahil eder. Ana `llm_service` ve `llm_cli` hedefleri, derlenen `libllama.so`'ya karşı linklenir.

```cmake
# Önemli Linkleme Komutu
target_link_libraries(llm_service PRIVATE
    proto_lib
    llama  # libllama.so'ya linklenir
    spdlog::spdlog
    # ... diğer vcpkg kütüphaneleri
)
```

### Docker Best Practices
-   **Multi-stage Build:** `builder` aşaması tüm derleme araçlarını ve ara dosyaları içerir. `runtime` aşaması ise sadece çalıştırılabilir dosyaları ve gerekli paylaşılan kütüphaneleri (`*.so`) içerir.
-   **Dinamik Kütüphane Yönetimi:** `runtime` imajı, `builder`'dan kopyalanan `*.so` dosyalarını bulabilmesi için `ldconfig` komutunu çalıştırır.

```dockerfile
# Dockerfile'dan kritik runtime adımları
FROM ubuntu:22.04 AS runtime

# ...
COPY --from=builder /app/build/llm_service /usr/local/bin/
COPY --from=builder /app/build/llm_cli /usr/local/bin/

# GEREKLİ TÜM paylaşılan kütüphaneleri kopyala
COPY --from=builder /app/build/bin/*.so /usr/local/lib/

# Dinamik linker önbelleğini güncelle
RUN ldconfig
# ...
```

## Kod Standartları

-   **C++17** standardı zorunludur.
-   **RAII** prensibi benimsenmelidir. `std::unique_ptr` ve `std::shared_ptr` kullanımı teşvik edilir.
-   **Header Guard:** Tüm header dosyaları `#pragma once` ile başlamalıdır.
-   **Exception Safety:** Fonksiyonlar, istisna durumlarında kaynak sızıntısı yapmamalıdır.

---

## Configuration

### Environment Variables

Servisi yapılandırmak için aşağıdaki ortam değişkenlerini kullanın. Bu değişkenler `docker-compose.yml` dosyasında veya production ortamınızda ayarlanabilir.

| Değişken                          | Açıklama                                                                | Varsayılan Değer              |
| --------------------------------- | ----------------------------------------------------------------------- | ----------------------------- |
| `LLM_LLAMA_SERVICE_IPV4_ADDRESS`  | Servisin dinleyeceği IP adresi. `0.0.0.0` tüm arayüzleri dinler.        | `0.0.0.0`                     |
| `LLM_LLAMA_SERVICE_HTTP_PORT`     | HTTP health check sunucusunun portu.                                    | `16070`                       |
| `LLM_LLAMA_SERVICE_GRPC_PORT`     | gRPC sunucusunun portu.                                                 | `16071`                       |
| `LLM_MODEL_PATH`                  | Konteyner içindeki GGUF model dosyasının tam yolu.                      | `/models/phi-3-mini.q4.gguf`  |
| `LLM_CONTEXT_SIZE`                | Modelin maksimum context penceresi.                                     | `4096`                        |
| `LLM_THREADS`                     | Token üretimi için kullanılacak CPU thread sayısı.                      | (Donanımın yarısı, max 8)     |
| `LOG_LEVEL`                       | Log seviyesi (`trace`, `debug`, `info`, `warn`, `error`, `critical`). | `info`                        |

**Örnek `docker-compose.yml` Yapılandırması:**

```yaml
# docker-compose.yml
services:
  llm-llama-service:
    # ...
    ports:
      - "127.0.0.1:16070:16070"
      - "127.0.0.1:16071:16071"
    environment:
      - LLM_LLAMA_SERVICE_IPV4_ADDRESS=0.0.0.0
      - LLM_LLAMA_SERVICE_HTTP_PORT=16070
      - LLM_LLAMA_SERVICE_GRPC_PORT=16071
      - LLM_THREADS=4
```

---
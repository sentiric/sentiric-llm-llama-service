# 👨‍💻 Geliştirici Rehberi

## Build Sistemi

### Bağımlılık Yönetimi
-   **vcpkg**: `gRPC`, `spdlog`, `httplib` gibi temel C++ bağımlılıklarını yönetir.
-   **`llama.cpp`**: Docker build sırasında sabitlenmiş bir commit'ten klonlanır ve anlık olarak derlenir. Proje, `llama.cpp`'yi bir alt dizin olarak kullanır.
-   **Protobuf**: `CMake`'in `FetchContent` modülü, `sentiric-contracts` reposunu derleme anında çeker ve `protoc` ile gRPC/Protobuf kodlarını üretir.

### CMake Yapılandırması
`CMakeLists.txt` dosyası, `vcpkg` toolchain'i kullanarak bağımlılıkları bulur. `llama.cpp` projesini `add_subdirectory` ile dahil eder ve `LLAMA_BUILD_COMMON=ON` bayrağını ayarlayarak `common` kütüphanesinin derlenmesini sağlar. `llm_service` ve `llm_cli` hedefleri, hem `llama` hem de `common` kütüphanelerine linklenir.

### Docker Best Practices
-   **Multi-stage Build:** `builder` aşaması tüm derleme araçlarını içerir. `runtime` aşaması ise sadece çalıştırılabilir dosyaları ve gerekli paylaşılan kütüphaneleri (`*.so`) içerir.
-   **Dinamik Kütüphane Yönetimi:** `runtime` imajı, `ldconfig` komutunu çalıştırarak dinamik linker önbelleğini günceller.

---

## Kod Standartları ve API Kullanımı

-   **C++17** standardı zorunludur.
-   **RAII** prensibi benimsenmelidir.
-   **`llama.cpp` API Kullanımı:** Projenin kullandığı `llama.cpp` versiyonu için tüm temel API kullanım desenleri **`docs/KB/04_LLAMA_CPP_API_BINDING.md`** dosyasında belgelenmiştir. Bu dosya, `llama.cpp` ile etkileşimde tek doğru kaynaktır.

---

## Geliştirme Sırasında Sık Kullanılan Komutlar

### Servisi Başlatma
```bash
# CPU için
docker compose up --build -d

# GPU için
docker compose -f docker-compose.yml -f docker-compose.gpu.yml -f docker-compose.gpu.override.yml up --build -d
```

### `llm_cli` Aracını Kullanma (mTLS Ortamında)

**ÖNEMLİ:** llm_cli aracını çalıştırmak için docker compose exec yerine docker compose run komutunu kullanın. docker-compose.override.yml dosyaları, llm-cli adında, mTLS için gerekli ortam değişkenlerini içeren özel bir servis tanımlar.

Bu yöntem, komutları basitleştirir ve ortam değişkeni sızıntısını önler.

Bunun için projenin kök dizininde, `docker-compose.yml` tarafından kullanılan tüm ortam değişkenlerini içeren bir `.env` dosyası oluşturun:

**`.env` dosyası örneği:**
```env
# Network
NETWORK_NAME=sentiric-net
NETWORK_SUBNET=10.88.0.0/16
NETWORK_GATEWAY=10.88.0.1
LLM_LLAMA_SERVICE_IPV4_ADDRESS=10.88.60.7

# Ports
LLM_LLAMA_SERVICE_HTTP_PORT=16070
LLM_LLAMA_SERVICE_GRPC_PORT=16071
LLM_LLAMA_SERVICE_METRICS_PORT=16072

# Security (mTLS) - Bu yollar, yerel makinenizdeki `sentiric-certificates` reposunun konumuna göre ayarlanmalıdır.
GRPC_TLS_CA_PATH=../sentiric-certificates/certs/ca.crt
LLM_LLAMA_SERVICE_CERT_PATH=../sentiric-certificates/certs/llm-llama-service-chain.crt
LLM_LLAMA_SERVICE_KEY_PATH=../sentiric-certificates/certs/llm-llama-service.key

# Diğer servis değişkenleri...
```

`.env` dosyası oluşturulduktan sonra, `llm_cli`'yi çalıştırmak için doğru komut şudur:

```bash
# --env-file parametresi, .env dosyasındaki değişkenleri exec komutuna aktarır
docker compose exec --env-file .env llm-llama-service llm_cli <komut>
```

**Örnek `generate` komutu (CPU veya GPU):**
```bash
# `run --rm` komutu, `llm-cli` servisini çalıştırır, komutu yürütür ve sonra konteyneri kaldırır.
docker compose run --rm llm-cli llm_cli generate "Merhaba dünya"
```

Örnek health komutu:

```bash
docker compose run --rm llm-cli llm_cli health
```

---

# 🚀 Deployment Rehberi

Bu servis, esnek bir `docker-compose` yapısı kullanarak farklı senaryolarda kolayca dağıtılabilir. Bu rehber, hem üretim (pre-built imajları kullanarak) hem de geliştirme (kaynaktan derleyerek) ortamları için adımları açıklar.

## Mimari Yaklaşımı: Temel + Profil + Geçersiz Kılma

Tekrarlardan kaçınmak ve yapılandırmayı basitleştirmek için aşağıdaki mimariyi kullanıyoruz:
- **`docker-compose.yml`:** Tüm ortak yapılandırmaları içeren temel dosyadır.
- **`docker-compose.cpu.yml` / `docker-compose.gpu.yml`:** Sadece CPU veya GPU'ya özel farkları (imaj adı, kaynaklar) tanımlayan "profil" dosyalarıdır.
- **`docker-compose.override.yml` / `docker-compose.gpu.override.yml`:** Sadece yerel geliştirme için kaynaktan derleme (`build`) talimatlarını içeren "geçersiz kılma" dosyalarıdır.

---

## 1. Üretim Dağıtımı (Pre-built İmajları Çekerek)

Bu senaryo, GitHub Container Registry'den (ghcr.io) hazır imajları çeker. En hızlı ve en kararlı yöntemdir.

### 1.1. CPU Üzerinde Çalıştırma

```bash
# Temel ve CPU profili dosyalarını kullanarak servisi başlat
# Bu komut, 'ghcr.io/sentiric/sentiric-llm-llama-service:latest' imajını çeker
docker compose -f docker-compose.yml -f docker-compose.cpu.yml up -d
```

### 1.2. GPU Üzerinde Çalıştırma (NVIDIA)

```bash
# Temel ve GPU profili dosyalarını kullanarak servisi başlat
# Bu komut, 'ghcr.io/sentiric/sentiric-llm-llama-service:latest-gpu' imajını çeker
docker compose -f docker-compose.yml -f docker-compose.gpu.yml up -d
```

---

## 2. Geliştirme Ortamı (Kaynaktan Derleyerek)

Bu senaryo, yerel kod değişikliklerinizi test etmek için kullanılır.

### 2.1. CPU Üzerinde Derleme ve Çalıştırma

`docker-compose.override.yml` dosyası, `docker compose` tarafından otomatik olarak algılanır.

```bash
# Bu komut, Dockerfile kullanarak yerel bir imaj oluşturur ve servisi başlatır
docker compose -f docker-compose.yml -f docker-compose.cpu.yml -f docker-compose.override.yml up --build -d

```

### 2.2. GPU Üzerinde Derleme ve Çalıştırma (NVIDIA)

GPU derlemesi için geçersiz kılma dosyasını manuel olarak belirtmemiz gerekir.

```bash
# Temel, GPU profili ve GPU geçersiz kılma dosyalarını birleştirerek servisi başlat
# Bu komut, Dockerfile.gpu kullanarak yerel bir imaj oluşturur
docker compose -f docker-compose.yml -f docker-compose.gpu.yml -f docker-compose.gpu.override.yml up --build -d
```

---

## 3. Servisi Durdurma

Hangi profille başlattığınızdan bağımsız olarak, servisi durdurmak için:

```bash
docker compose down
```

## 4. Kaynak Gereksinimleri ve Ölçeklendirme Notları

Bu servis, `LlamaContextPool` mimarisi sayesinde gerçek eşzamanlılık sunar. Ancak bu, kaynak kullanımı üzerinde doğrudan bir etkiye sahiptir.

### Temel Formül

Gerekli toplam bellek (RAM veya VRAM), aşağıdaki formülle kabaca tahmin edilebilir:

**Toplam Bellek ≈ Model Boyutu + ( `LLM_LLAMA_SERVICE_THREADS` × Her Context için KV Cache Boyutu )**

-   **Model Boyutu:** Kullandığınız GGUF dosyasının boyutu.
-   **KV Cache Boyutu:** Bu, `context_size` ve modelin mimarisine bağlıdır. `phi-3-mini-4k-instruct-q4.gguf` için `4096` context ile yaklaşık **1.5 GB**'tır.

### Örnek Senaryolar

-   **CPU (16GB RAM):** `LLM_LLAMA_SERVICE_THREADS=3` ayarıyla:
    `~2.3 GB (Model) + (3 * ~1.5 GB) = ~6.8 GB` RAM sadece servis için gereklidir. İşletim sistemi ve diğer servislerle birlikte bu, 16 GB'lık bir sistemde yavaşlamaya (swapping) neden olabilir. Eşzamanlılık seviyesini, mevcut sistem belleğine göre dikkatli bir şekilde ayarlayın.

-   **GPU (6GB VRAM):** `LLM_LLAMA_SERVICE_THREADS=3` ve `n_gpu_layers=-1` (tam offload) ayarıyla:
    `~2.3 GB (Model) + (3 * ~1.5 GB) = ~6.8 GB` VRAM gereklidir. Bu, 6 GB VRAM'i aşar ve `out of memory` hatasına yol açar.
    -   **Çözüm:** Bu donanımda eşzamanlılığı sağlamak için ya `LLM_LLAMA_SERVICE_THREADS` sayısını `1` veya `2` gibi VRAM'e sığacak bir değere düşürün ya da modelin sadece bir kısmını GPU'ya offload edin (`n_gpu_layers`).

**Öneri:** Üretim ortamında, hedeflenen eşzamanlılık seviyesine yetecek kadar RAM veya VRAM kaynağı ayarlanmalıdır. Geliştirme ortamında, `LLM_LLAMA_SERVICE_THREADS` değişkenini `1` olarak ayarlayarak sıralı ama stabil bir şekilde çalışabilirsiniz.

## Configuration

### Environment Variables

Servisi yapılandırmak için aşağıdaki ortam değişkenlerini kullanın. Tüm değişkenler `LLM_LLAMA_SERVICE_` öneki ile başlar.

| Değişken                                   | Açıklama                                                                          | Varsayılan Değer                  |
| ------------------------------------------ | --------------------------------------------------------------------------------- | --------------------------------- |
| **Network**                                |                                                                                   |                                   |
| `LLM_LLAMA_SERVICE_LISTEN_ADDRESS`           | Servisin dinleyeceği IP adresi. `0.0.0.0` tüm arayüzleri dinler.                  | `0.0.0.0`                         |
| `LLM_LLAMA_SERVICE_HTTP_PORT`              | HTTP health check sunucusunun portu.                                              | `16070`                           |
| `LLM_LLAMA_SERVICE_GRPC_PORT`              | gRPC sunucusunun portu.                                                           | `16071`                           |
| **Model Management**                       |                                                                                   |                                   |
| `LLM_LLAMA_SERVICE_MODEL_DIR`              | Modellerin indirileceği ve saklanacağı konteyner içindeki dizin.                  | `/models`                         |
| `LLM_LLAMA_SERVICE_MODEL_ID`               | Hugging Face repo ID'si (ör: `microsoft/Phi-3-mini-4k-instruct-gguf`).          | `microsoft/Phi-3-mini-4k-instruct-gguf` |
| `LLM_LLAMA_SERVICE_MODEL_FILENAME`         | İndirilecek GGUF dosyasının tam adı.                                              | `Phi-3-mini-4k-instruct-q4.gguf`  |
| `LLM_LLAMA_SERVICE_MODEL_PATH`             | *[Legacy]* `MODEL_ID` belirtilmezse kullanılacak modelin tam yolu.                  | `""`                              |
| **Engine & Performance**                   |                                                                                   |                                   |
| `LLM_LLAMA_SERVICE_GPU_LAYERS`             | GPU'ya yüklenecek model katmanı sayısı. `-1` tüm katmanları yükler.               | `0`                               |
| `LLM_LLAMA_SERVICE_CONTEXT_SIZE`           | Modelin maksimum context penceresi.                                               | `4096`                            |
| `LLM_LLAMA_SERVICE_THREADS`                | Token üretimi için kullanılacak CPU thread sayısı.                                | (Donanımın yarısı, max 8)         |
| **Logging**                                |                                                                                   |                                   |
| `LLM_LLAMA_SERVICE_LOG_LEVEL`              | Log seviyesi (`trace`, `debug`, `info`, `warn`, `error`, `critical`).           | `info`                            |
| **Default Sampling**                       | *gRPC isteğinde belirtilmezse kullanılacak varsayılan değerler.*                    |                                   |
| `LLM_LLAMA_SERVICE_DEFAULT_MAX_TOKENS`     | Varsayılan maksimum üretilecek token sayısı.                                      | `1024`                            |
| `LLM_LLAMA_SERVICE_DEFAULT_TEMPERATURE`    | Varsayılan sampling sıcaklığı. Yaratıcılığı artırır.                               | `0.8`                             |
| `LLM_LLAMA_SERVICE_DEFAULT_TOP_K`          | Varsayılan Top-K sampling değeri.                                                 | `40`                              |
| `LLM_LLAMA_SERVICE_DEFAULT_TOP_P`          | Varsayılan Top-P (nucleus) sampling değeri.                                       | `0.95`                            |
| `LLM_LLAMA_SERVICE_DEFAULT_REPEAT_PENALTY` | Varsayılan tekrar cezası. Tekrar eden metinleri engeller.                           | `1.1`                             |


**Örnek `docker-compose.yml` Yapılandırması:**

```yaml
# docker-compose.yml
services:
  llm-llama-service:
    # ...
    environment:
      - LLM_LLAMA_SERVICE_LISTEN_ADDRESS=0.0.0.0
      - LLM_LLAMA_SERVICE_HTTP_PORT=16070
      - LLM_LLAMA_SERVICE_GRPC_PORT=16071
      - LLM_LLAMA_SERVICE_THREADS=1
      - LLM_LLAMA_SERVICE_LOG_LEVEL=info
```

### Resource Limits
```yaml
# docker-compose.yml
deploy:
  resources:
    limits:
      memory: 4G
      cpus: '2.0'
    reservations:
      memory: 2G
      cpus: '1.0'
```

## Monitoring

### Health Checks
```yaml
healthcheck:
  test: ["CMD", "curl", "-f", "http://localhost:16070/health"]
  interval: 30s
  timeout: 10s
  retries: 3
  start_period: 5m
```

### Log Management
```bash
# Log rotation
docker run --log-driver json-file \
  --log-opt max-size=10m \
  --log-opt max-file=3

# Log monitoring
docker logs -f --tail 100 llm-llama-service
```

## Scaling

### Vertical Scaling
- **Memory**: 8GB for larger models
- **CPU**: More cores for parallel processing
- **Storage**: SSD for faster model loading

### Horizontal Scaling
```yaml
# docker-compose.yml
deploy:
  replicas: 1
  # Note: Stateful service, scaling requires careful design
```

## Backup & Recovery

### Model Backup
```bash
# Backup model
cp models/phi-3-mini.q4.gguf /backup/

# Restore model
cp /backup/phi-3-mini.q4.gguf models/
```

### Configuration Backup
```bash
# Backup configuration
tar czf config-backup.tar.gz \
  docker-compose.yml \
  models/download.sh \
  src/config.h
```

## Security Hardening

### Container Security
```yaml
# docker-compose.yml
security_opt:
  - no-new-privileges:true
read_only: true
tmpfs:
  - /tmp
```

### Network Security
```yaml
# Only expose necessary ports
ports:
  - "127.0.0.1:16070:16070"  # Local only
  - "127.0.0.1:16071:16071"  # Local only
```

## Maintenance

### Updates
```bash
# Service update
git pull origin main
docker compose down
docker compose up --build -d

# Model update
rm models/phi-3-mini.q4.gguf
./models/download.sh
docker compose restart
```

### Cleanup
```bash
# Clean Docker
docker system prune -f
docker volume prune -f

# Log cleanup
find /var/lib/docker/containers -name "*.log" -mtime +7 -delete
```

## Troubleshooting

### Common Issues
- **Port conflicts**: Change ports in docker-compose.yml
- **Memory issues**: Increase resource limits
- **Model corruption**: Re-download model

### Recovery Script
```bash
#!/bin/bash
# recovery.sh
docker compose down
docker system prune -f
./models/download.sh
docker compose up -d
echo "Recovery completed"
```

---
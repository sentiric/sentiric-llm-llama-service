# 🚀 Deployment Rehberi

Bu servis, GitHub Actions aracılığıyla otomatik olarak `ghcr.io/sentiric/sentiric-llm-llama-service` adresine Docker imajı olarak yayınlanır. Üretim ortamında dağıtım yapmanın en iyi yolu bu önceden oluşturulmuş imajı kullanmaktır.

## Sistem Gereksinimleri
-   **Docker**: 20.10+
-   **Docker Compose**: 2.0+ (opsiyonel, `docker run` da kullanılabilir)
-   **RAM**: 4GB minimum
-   **Depolama**: Model dosyası için ~3GB boş alan

## Üretim Dağıtımı (Production Deployment)

1.  **`docker-compose.yml` Dosyasını Hazırlayın:**
    Projenin kök dizinindeki `docker-compose.yml` dosyası, üretim dağıtımı için tasarlanmıştır. Bu dosyayı sunucunuza kopyalayın.

2.  **Ortam Değişkenlerini Ayarlayın (Opsiyonel):**
    Gerekirse, `docker-compose.yml` dosyasının yanına bir `.env` dosyası oluşturarak veya doğrudan sistem ortam değişkenlerini ayarlayarak konfigürasyonu özelleştirin. (Tüm değişkenler için `Configuration` bölümüne bakın.)

3.  **Servisi Başlatın:**
    `docker-compose.yml`'nin bulunduğu dizinde aşağıdaki komutu çalıştırın.

    ```bash
    # En güncel imajı çek ve servisi başlat
    docker compose up -d
    ```
    Bu komut, `build` yapmaz, bunun yerine GHCR'den `:latest` etiketli imajı çeker.

4.  **Doğrulama:**
    Servisin başlaması, modelin indirilmesi nedeniyle birkaç dakika sürebilir.

    ```bash
    # Konteyner durumunu kontrol et
    docker compose ps

    # Servis hazır olduğunda health check yap
    curl http://localhost:16070/health
    ```

## Geliştirme Ortamı (Development)

Geliştirme yaparken kaynak kodundan build yapmak için, projenin içindeki `docker-compose.override.yml` dosyası otomatik olarak kullanılır.

```bash
# Geliştirme ortamında, yerel kaynak kodunu kullanarak build et ve başlat
docker compose up --build -d
```

## Configuration

### Environment Variables

Servisi yapılandırmak için aşağıdaki ortam değişkenlerini kullanın. Tüm değişkenler `LLM_LLAMA_SERVICE_` öneki ile başlar.

| Değişken                                   | Açıklama                                                                          | Varsayılan Değer                  |
| ------------------------------------------ | --------------------------------------------------------------------------------- | --------------------------------- |
| **Network**                                |                                                                                   |                                   |
| `LLM_LLAMA_SERVICE_IPV4_ADDRESS`           | Servisin dinleyeceği IP adresi. `0.0.0.0` tüm arayüzleri dinler.                  | `0.0.0.0`                         |
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
      - LLM_LLAMA_SERVICE_IPV4_ADDRESS=0.0.0.0
      - LLM_LLAMA_SERVICE_HTTP_PORT=16070
      - LLM_LLAMA_SERVICE_GRPC_PORT=16071
      - LLM_LLAMA_SERVICE_THREADS=4
      - LLM_LLAMA_SERVICE_LOG_LEVEL=debug
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
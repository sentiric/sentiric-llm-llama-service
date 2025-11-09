# 🐛 Sorun Giderme Rehberi

## Hata Kataloğu

### 1. Library Loading Errors

**Hata**: `libllama.so: cannot open shared object file`
**Sebep**: Dynamic linking, static build değil
**Çözüm**:
```cmake
# CMakeLists.txt
set(LLAMA_STATIC ON)
set(BUILD_SHARED_LIBS OFF)
# ggml_static SİLİNECEK, sadece llama kalacak
```

**Hata**: `libgomp.so.1: cannot open shared object file`
**Sebep**: OpenMP runtime eksik
**Çözüm**:
```dockerfile
# Dockerfile
RUN apt-get install -y libgomp1
```

### 2. Model Loading Issues

**Hata**: Model yüklenemiyor
**Kontrol Listesi**:
- ✅ Model dosyası var mı? `ls models/`
- ✅ Disk alanı yeterli mi? `df -h`
- ✅ Model path doğru mu? `LLM_LOCAL_SERVICE_MODEL_PATH`
- ✅ Permissions doğru mu? `chmod 644 models/*.gguf`

### 3. Build Failures

**Hata**: `ggml_static not found`
**Sebep**: Yeni llama.cpp'de bu library kaldırıldı
**Çözüm**:
```cmake
# ESKİ (SİLİNECEK)
target_link_libraries(llm_service PRIVATE llama ggml_static)

# YENİ
target_link_libraries(llm_service PRIVATE llama)
```

**Hata**: Protobuf compilation failed
**Sebep**: vcpkg bağımlılıkları eksik
**Çözüm**:
```bash
docker compose down
docker system prune -f
docker compose up --build -d
```

### 4. Runtime Issues

**Hata**: Service restarting loop
**Kontrol**:
```bash
docker logs llm-llama-service
docker exec -it llm-llama-service ldd /usr/local/bin/llm_service
```

**Hata**: GRPC connection failed
**Kontrol**:
```bash
# Port açık mı?
netstat -tulpn | grep 16061

# Container çalışıyor mu?
docker ps | grep llm-llama-service
```

## Performance Issues

### High Memory Usage
- **Normal**: ~2.5GB (model + KV cache)
- **Anormal**: >4GB - memory leak şüphesi

### Slow Generation
- **Beklenen**: ~50 tokens/saniye
- **Yavaş**: <10 tokens/saniye - CPU throttle

### Debug Commands
```bash
# Memory usage
docker stats llm-llama-service

# CPU usage
top -p $(docker inspect llm-llama-service --format '{{.State.Pid}}')

# Model loading time
grep "LLM Engine initialized" docker.log
```

## Recovery Procedures

### Complete Reset
```bash
# Nuclear option
docker compose down
docker system prune -af
docker volume prune -f
./models/download.sh  # Re-download model
docker compose up --build -d
```

### Model Corruption
```bash
# Re-download model
rm models/phi-3-mini.q4.gguf
./models/download.sh
```

## Monitoring

### Health Metrics
```bash
# Automated health check
curl -s http://localhost:16060/health | jq '.model_ready'

# Response time
time curl -s http://localhost:16060/health > /dev/null
```

### Log Analysis
```bash
# Error patterns
docker logs llm-llama-service | grep -i error

# Performance issues
docker logs llm-llama-service | grep -i "slow\|timeout"
```

## Prevention

### Build Time
- Static linking kullan
- libgomp1 runtime ekle
- Multi-stage Docker build

### Runtime
- Health monitoring
- Resource limits
- Log aggregation
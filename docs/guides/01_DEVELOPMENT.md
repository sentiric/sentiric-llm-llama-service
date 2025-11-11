# 👨‍💻 Geliştirici Rehberi

## Yeni Build Sistemi

### Bağımlılık Yönetimi
- **llama.cpp**: Docker build sırasında otomatik indirilir
- **vcpkg**: Bağımlılıkları yönetir
- **No Submodules**: Repo daha hafif ve temiz

### Geliştirme Avantajları
- ✅ Daha hızlı git clone
- ✅ Submodule conflict yok
- ✅ Otomatik dependency management
- ✅ Reproducible builds

## Kod Standartları

### C++ Standartları
- **C++17** standardı
- **RAII** pattern kullanımı
- **Smart pointers** (`std::shared_ptr`, `std::unique_ptr`)
- **Exception safety** garantisi

### Header Dosyaları
```cpp
// Örnek header structure
#pragma once
#include <memory>
#include <string>
#include "llama.h"

class LLMEngine {
public:
    explicit LLMEngine(const Settings& settings);
    ~LLMEngine();
    
    // Rule of Five
    LLMEngine(const LLMEngine&) = delete;
    LLMEngine& operator=(const LLMEngine&) = delete;
    
private:
    llama_model* model_ = nullptr;
    std::atomic<bool> model_loaded_{false};
};
```

## Build System

### CMake Yapılandırması
```cmake
# KRİTİK: Bu flag'ler değiştirilmemeli
set(LLAMA_STATIC ON)
set(BUILD_SHARED_LIBS OFF)

# Bağımlılıklar
target_link_libraries(llm_service PRIVATE
    proto_lib
    llama  # SADECE llama, ggml_static DEĞİL!
    spdlog::spdlog
    gRPC::grpc++
)
```

### Docker Best Practices
```dockerfile
# Multi-stage build
FROM ubuntu:22.04 AS builder
# ... build steps

FROM ubuntu:22.04
# Sadece runtime gereksinimleri
RUN apt-get install -y libgomp1
COPY --from=builder /app/build/llm_service /usr/local/bin/
```

## Testing

### Unit Testler
```bash
# Build test executable
cmake --build build --target grpc_test_client

# Manual test
docker compose exec llm-llama-service grpc_test_client "Test prompt"
```

### Integration Test
```bash
#!/bin/bash
# test_integration.sh

echo "🧪 Integration Test Başlıyor..."

# Health check
curl -f http://localhost:16060/health || exit 1

# GRPC test
docker compose exec llm-llama-service grpc_test_client "Test" || exit 1

echo "✅ Tüm testler başarılı!"
```

## Debugging

### Common Issues

1. **Model Loading Failed**
   ```bash
   # Check model file
   ls -la models/phi-3-mini.q4.gguf
   
   # Check disk space
   df -h
   ```

2. **GRPC Connection Issues**
   ```bash
   # Check port binding
   netstat -tulpn | grep 16061
   
   # Check container logs
   docker logs llm-llama-service
   ```

3. **Performance Issues**
   ```bash
   # Monitor resources
   docker stats llm-llama-service
   
   # Check model loading time
   grep "LLM Engine initialized" docker.log
   ```

## Performance Optimization

### Memory Management
- **KV Cache**: 1536MB sabit
- **Model Weights**: MMAP ile lazy loading
- **Threading**: 4 thread optimal

### Sampling Optimizations
```cpp
// Current: Greedy sampling
// Future: Temperature, Top-K, Top-P
llama_token new_token_id = 0;
float max_logit = logits[0];
for (int i = 1; i < n_vocab; ++i) {
    if (logits[i] > max_logit) {
        max_logit = logits[i];
        new_token_id = i;
    }
}
```

## Deployment Checklist

- [ ] Model dosyası mevcut
- [ ] Docker image build edildi
- [ ] Health check çalışıyor
- [ ] GRPC streaming çalışıyor
- [ ] Memory usage normal
- [ ] Loglar temiz
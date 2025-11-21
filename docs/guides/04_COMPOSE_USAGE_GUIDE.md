# 🐳 Docker Compose Kullanım Rehberi

## 📋 Genel Bakış

Bu proje, farklı kullanım senaryoları için optimize edilmiş multiple Docker Compose dosyaları kullanır:

### 🏗️ Compose Dosya Mimarisi
- **`docker-compose.yml`** - Temel yapılandırma (asla tek başına kullanılmaz)
- **`docker-compose.cpu.yml`** - Production CPU profili
- **`docker-compose.gpu.yml`** - Production GPU profili  
- **`docker-compose.override.yml`** - Local development (CPU)
- **`docker-compose.gpu.override.yml`** - Local development (GPU)
- **`docker-compose.run.gpu.yml`** - CLI için GPU konteyneri
- **`docker-compose.open-webui.yml`** - Open WebUI Arayüzü

---

## 🚀 KULLANIM SENARYOLARI

### 1. 🖥️ PRODUCTION - CPU Dağıtımı

**Amaç:** Pre-built imaj ile production CPU ortamı

```bash
# GitHub Container Registry'den imajı çek ve çalıştır
docker compose -f docker-compose.yml -f docker-compose.cpu.yml up -d

# Servisi durdur
docker compose -f docker-compose.yml -f docker-compose.cpu.yml down
```

**Özellikler:**
- ✅ Pre-built imaj kullanır (`ghcr.io/sentiric/sentiric-llm-llama-service:latest`)
- ✅ Optimize edilmiş CPU ayarları
- ✅ Production-ready health checks
- ✅ mTLS güvenlik aktif

---

### 2. 🎮 PRODUCTION - GPU Dağıtımı

**Amaç:** Pre-built imaj ile production GPU ortamı

```bash
# GPU imajını çek ve çalıştır
docker compose -f docker-compose.yml -f docker-compose.gpu.yml up -d

# Servisi durdur
docker compose -f docker-compose.yml -f docker-compose.gpu.yml down
```

**GPU Ayarları:**
```yaml
- LLM_LLAMA_SERVICE_GPU_LAYERS=28
- NVIDIA_VISIBLE_DEVICES=all
```

**Özellikler:**
- ✅ Pre-built GPU imajı
- ✅ NVIDIA GPU desteği
- ✅ Optimize edilmiş 6GB VRAM ayarları
- ✅ Otomatik GPU resource allocation

---

### 3. 🔧 LOCAL DEVELOPMENT - CPU Geliştirme

**Amaç:** Yerel kod değişiklikleriyle development

```bash
# Kaynaktan build et ve çalıştır (override otomatik kullanılır)
docker compose up --build -d

# Logları izle
docker compose logs -f llm-llama-service

# Servisi durdur
docker compose down
```

**Özellikler:**
- ✅ Yerel kod değişiklikleriyle otomatik rebuild
- ✅ `docker-compose.override.yml` otomatik kullanılır
- ✅ Geliştirme için optimize edilmiş ayarlar
- ✅ CLI konteyneri dahil

---

### 4. 🎯 LOCAL DEVELOPMENT - GPU Geliştirme

**Amaç:** Yerel GPU ile development ve test

```bash
# GPU için build et ve çalıştır
docker compose -f docker-compose.yml -f docker-compose.gpu.yml -f docker-compose.gpu.override.yml up --build -d

# Logları izle
docker compose -f docker-compose.yml -f docker-compose.gpu.yml -f docker-compose.gpu.override.yml logs -f

# Servisi durdur
docker compose -f docker-compose.yml -f docker-compose.gpu.yml -f docker-compose.gpu.override.yml down
```

**Optimize GPU Ayarları (6GB VRAM):**
```yaml
- LLM_LLAMA_SERVICE_GPU_LAYERS=28
- LLM_LLAMA_SERVICE_CONTEXT_SIZE=1024
- LLM_LLAMA_SERVICE_THREADS=1
- LLM_LLAMA_SERVICE_THREADS_BATCH=1
- LLM_LLAMA_SERVICE_ENABLE_BATCHING=false
- LLM_LLAMA_SERVICE_ENABLE_WARM_UP=true
```

---

### 5. 🌐 OPEN WEBUI ENTEGRASYONU (YENİ)

**Amaç:** Servisi test etmek veya kullanmak için modern, ChatGPT benzeri bir arayüz başlatmak.

```bash
# WebUI'yi Başlat (Port: 3000)
make up-ui

# Veya Manuel Olarak:
docker compose -f docker-compose.open-webui.yml up -d
```

**Erişim:**
- Tarayıcı: `http://localhost:3000`
- İlk açılışta bir yönetici hesabı oluşturmanız istenecektir.

**Özellikler:**
- ✅ `sentiric-net` ağına otomatik bağlanır.
- ✅ Modelleri otomatik tanır (`/v1/models` üzerinden).
- ✅ Veriler `open-webui` volume'ünde kalıcı saklanır.

---

### 6. 🛠️ CLI ARAÇLARI - GPU Ortamında

**Amaç:** GPU destekli CLI komutlarını çalıştırma

```bash
# Health check
docker compose -f docker-compose.run.gpu.yml run --rm llm-cli llm_cli health

# Metin üretme
docker compose -f docker-compose.run.gpu.yml run --rm llm-cli llm_cli generate "Merhaba dünya"

# RAG testi (run_request.sh alternatifi)
docker compose -f docker-compose.run.gpu.yml run --rm llm-cli llm_cli generate "Soru" --rag-context "Context metni"
```

**Özellikler:**
- ✅ Geçici konteyner (--rm)
- ✅ GPU erişimi
- ✅ mTLS sertifikaları
- ✅ Servis ağına bağlı

---

## ⚙️ ORTAM DEĞİŞKENLERİ REFERANSI

### Temel Ayarlar
```bash
# Model
LLM_LLAMA_SERVICE_MODEL_ID=ggml-org/gemma-3-1b-it-qat-GGUF
LLM_LLAMA_SERVICE_MODEL_FILENAME=gemma-3-1b-it-qat-Q4_0.gguf

# Performans
LLM_LLAMA_SERVICE_THREADS=1
LLM_LLAMA_SERVICE_CONTEXT_SIZE=1024
LLM_LLAMA_SERVICE_GPU_LAYERS=0  # CPU: 0, GPU: 28
```

### Yeni Gelişmiş Özellikler
```bash
# Warm-up (Önerilen: true)
LLM_LLAMA_SERVICE_ENABLE_WARM_UP=true

# Dynamic Batching (Sadece THREADS > 1 için)
LLM_LLAMA_SERVICE_ENABLE_BATCHING=false
LLM_LLAMA_SERVICE_MAX_BATCH_SIZE=1
LLM_LLAMA_SERVICE_BATCH_TIMEOUT_MS=10
```

### Güvenlik
```bash
# mTLS sertifikaları
GRPC_TLS_CA_PATH=../sentiric-certificates/certs/ca.crt
LLM_LLAMA_SERVICE_CERT_PATH=../sentiric-certificates/certs/llm-llama-service-chain.crt
LLM_LLAMA_SERVICE_KEY_PATH=../sentiric-certificates/certs/llm-llama-service.key
```

---

## 🚨 SIK KARŞILAŞILAN SORUNLAR

### 1. "orphan containers" Uyarısı
```bash
# Çözüm: --remove-orphans kullan
docker compose down --remove-orphans
```

### 2. GPU Bellek Hatası
```bash
# Çözüm: GPU layers azalt
LLM_LLAMA_SERVICE_GPU_LAYERS=24
LLM_LLAMA_SERVICE_THREADS=1
```

### 3. mTLS Bağlantı Hatası
```bash
# Çözüm: Sertifika yollarını kontrol et
ls -la ../sentiric-certificates/certs/
```

### 4. Open WebUI "Network Problem" Hatası
**Neden:** WebUI konteyneri LLM servisine ulaşamıyor.
**Çözüm:** `make up-ui` kullanın veya her iki konteynerin de `sentiric-net` ağında olduğundan emin olun. URL olarak `http://llm-llama-service:16070` kullanın.

---

## 🔄 HIZLI REFERANS TABLOSU

| Senaryo | Komut | Build | GPU | Use Case |
|---------|--------|--------|-----|----------|
| Production CPU | `-f docker-compose.yml -f docker-compose.cpu.yml` | ❌ | ❌ | Production |
| Production GPU | `-f docker-compose.yml -f docker-compose.gpu.yml` | ❌ | ✅ | Production |
| Dev CPU | `docker compose up --build -d` | ✅ | ❌ | Geliştirme |
| Dev GPU | `-f docker-compose.yml -f docker-compose.gpu.yml -f docker-compose.gpu.override.yml` | ✅ | ✅ | Geliştirme |
| Open WebUI | `make up-ui` | ❌ | ❌ | UI / Chat |
| CLI GPU | `-f docker-compose.run.gpu.yml run --rm llm-cli` | ❌ | ✅ | Test |

---

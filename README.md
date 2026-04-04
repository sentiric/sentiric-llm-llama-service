# 🧠 Sentiric LLM Llama Service

[![Production Ready](https://img.shields.io/badge/status-production%20ready-success.svg)]()
[![Architecture](https://img.shields.io/badge/arch-C%2B%2B17_Native-blue.svg)]()

**Sentiric LLM Engine**, `llama.cpp` üzerine inşa edilmiş; RAG, Dynamic Batching ve Smart Context Caching yeteneklerine sahip yüksek performanslı yerel (Local) Büyük Dil Modeli (LLM) çıkarım sunucusudur.

## 🚀 Hızlı Başlangıç

### 1. Çalıştırma (Docker GPU - Önerilen)
```bash
docker compose -f docker-compose.yml -f docker-compose.gpu.yml up -d
```

### 2. Doğrulama (Health Check)
```bash
curl http://localhost:16070/health
# Beklenen Çıktı: {"status":"healthy","model_ready":true,"capacity":{"active":0,"available":4,"total":4}}
```

## 🏛️ Mimari Anayasa ve Kılavuzlar
* **Kodlama Kuralları (AI/İnsan):** Bu repoda kod geliştirmeden önce GİZLİ [.context.md](.context.md) dosyasını okuyun.
* **Çekirdek Algoritmalar (C++):** Smart Caching, VRAM yönetimi ve Context Shifting matematiği için [LOGIC.md](LOGIC.md) dosyasını inceleyin.
* **Sistem Sınırları ve Topoloji:** Bu servisin platformdaki konumu ve dış bağlantıları **[sentiric-spec](https://github.com/sentiric/sentiric-spec)** anayasasında tanımlıdır.
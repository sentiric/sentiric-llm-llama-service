# 🧠 Sentiric LLM Llama Service (v2.5)

**Production-Ready**, yüksek performanslı, C++ tabanlı yerel LLM çıkarım motoru. Özellikle **Gerçek Zamanlı Telefon Asistanı (Voice AI)** senaryoları için optimize edilmiş, **Qwen 2.5 3B** motoru ile güçlendirilmiştir.

[![CI - Build and Push Docker Image](https://github.com/sentiric/sentiric-llm-llama-service/actions/workflows/build-and-push.yml/badge.svg)](https://github.com/sentiric/sentiric-llm-llama-service/actions/workflows/build-and-push.yml)

## 🚀 Durum: STABLE (Üretim Hazır)

Bu servis, **15 Aralık 2025** itibarıyla v2.5 sürümüne yükseltilmiş ve aşağıdaki kritik yeteneklerle donatılmıştır:

-   ✅ **Phone-Call Ready Latency:** GPU hızlandırması ile <300ms ilk token süresi (TTFT).
-   ✅ **Qwen 2.5 3B Core:** Türkçe dil desteği ve talimat takibi (Instruction Following) için sınıfının lideri.
-   ✅ **RAG (Retrieval-Augmented Generation):** Dış bağlam verileriyle (Context Injection) halüsinasyonsuz yanıtlar.
-   ✅ **Smart Context Pooling:** Aynı anda çoklu aramayı yönetebilen, thread-safe havuz mimarisi.
-   ✅ **Deep Observability:** Prometheus metrikleri, Trace ID takibi ve detaylı yapılandırılmış loglar.

---

## 🛠️ Hızlı Başlangıç

### Ön Gereksinimler
-   Docker & Docker Compose
-   NVIDIA GPU (Tavsiye edilen: 6GB VRAM ve üzeri)
-   CUDA Toolkit 12.0+

### 1. Başlatma (Otomatik Kurulum)

Sistem, ilk açılışta gerekli `Qwen 2.5 3B` modelini otomatik olarak indirir.

```bash
# Servisi ve veritabanlarını başlat
make up

# Logları izle (Model indirme sürecini görmek için)
make logs
```

### 2. Sağlık Kontrolü

Model yüklendiğinde servis `Healthy` durumuna geçer:

```bash
curl http://localhost:16070/health
# Yanıt: {"status": "healthy", "model_ready": true, "capacity": ...}
```

### 3. Test Etme (CLI ile)

Dahili test aracı ile bir RAG sorgusu gönderin:

```bash
# Sigorta senaryosu örneği
docker compose -f docker-compose.run.gpu.yml run --rm llm-cli \
  llm_cli generate "Mehmet Bey'in poliçesi ne durumda?" \
  --rag-context "Müşteri: Mehmet Aslan. Poliçe: Aktif. Bitiş: 2026."
```

---

## ⚙️ Yapılandırma ve Profiller

Servis, statik model ayarları (`profiles.json`) ile dinamik altyapı ayarlarını (`.env`) birbirinden ayırır.

### Aktif Model Profili: `qwen25_3b_phone_assistant`

Bu profil, telefon görüşmeleri için özel olarak ayarlanmıştır:
-   **Model:** Qwen 2.5 3B Instruct
-   **Temperature:** 0.2 (Tutarlı ve ciddi yanıtlar için)
-   **Context Size:** 8192 Token
-   **System Prompt:** Çağrı merkezi asistanı kimliği.

*Farklı bir profil kullanmak için `models/profiles.json` dosyasını inceleyin.*

### Temel Ortam Değişkenleri (`.env`)

| Değişken | Varsayılan | Açıklama |
|---|---|---|
| `LLM_LLAMA_SERVICE_GPU_LAYERS` | `100` | GPU'ya yüklenecek katman sayısı (100 = Tümü). |
| `LLM_LLAMA_SERVICE_THREADS` | `Auto` | İşlemci çekirdek limiti. |
| `LLM_LLAMA_SERVICE_KV_OFFLOAD` | `true` | KV Cache'i VRAM'de tut (Hız için kritik). |
| `LLM_LLAMA_SERVICE_PORT_GRPC` | `16071` | Ana iletişim portu. |

---

## 🏗️ Mimari

```mermaid
graph TD
    Client[Gateway / Voice Service] -->|gRPC (mTLS)| GRPC_Server
    
    subgraph "LLM Service Container"
        GRPC_Server --> Engine[LLM Engine]
        HTTP_Server --> Engine
        
        Engine --> Batcher[Dynamic Batcher]
        Batcher --> Pool[Llama Context Pool]
        
        Pool -->|Acquire| GPU[(NVIDIA GPU)]
        Pool -->|Load| ModelFile[Qwen 2.5 GGUF]
    end
```

### Temel Bileşenler
1.  **Dynamic Batcher:** Gelen istekleri mikrosaniyeler içinde gruplayarak GPU verimini artırır.
2.  **Context Pool:** Her telefon görüşmesi için izole bir bellek alanı (Context) sağlar.
3.  **Prompt Formatter:** RAG verisini ve geçmişi modelin anlayacağı özel formata çevirir.

---

## 📊 Performans Referansları

**Donanım:** NVIDIA RTX 3060 (6GB VRAM)

| Metrik | Değer | Açıklama |
|---|---|---|
| **TTFT (Time To First Token)** | ~250ms | İlk sesin çıkma süresi. |
| **TPS (Tokens Per Second)** | ~55-60 | Konuşma hızı (İnsan ortalamasının 3 katı). |
| **Max Concurrent Calls** | 4-5 | Aynı anda desteklenen aktif görüşme. |

---

## 📜 Lisans

Bu proje **AGPL-3.0** lisansı altındadır.
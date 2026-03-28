# 🧠 Sentiric LLM Llama Service (v3.2.1)

**Production-Ready**, yüksek performanslı, C++ tabanlı yerel LLM çıkarım motoru. **Google Gemma 3 4B** ve **Qwen 2.5** mimarileri için optimize edilmiş, RAG (Retrieval-Augmented Generation) tabanlı sesli asistan altyapısı.

[![CI - Build and Push Docker Image](https://github.com/sentiric/sentiric-llm-llama-service/actions/workflows/build-and-push.yml/badge.svg)](https://github.com/sentiric/sentiric-llm-llama-service/actions/workflows/build-and-push.yml)

## 🚀 Durum: STABLE (Üretim Hazır)

Bu servis, **v3.2.1** sürümüyle aşağıdaki kritik yeteneklere kavuşmuştur:

-   ✅ **Surgical Logging & Folding:** SUTS v4.0 JSON formatı ve `trace_id`, `span_id` iz sürme mimarisi entegrasyonu.
-   ✅ **Strict Tenant Isolation:** gRPC ve HTTP seviyesinde Fail-Fast kiracı izolasyonu.
-   ✅ **Ultra-Low Latency:** Gemma 3 optimizasyonu ile **250ms - 500ms** arası ilk token süresi (TTFT).
-   ✅ **Smart Context Caching:** Benzer sorgularda önbellek kullanımı ile anında yanıt.
-   ✅ **Robust Validation:** Sağlık, Finans, Turizm gibi dikey sektörler için 14 farklı test senaryosu.

-   ✅ **Secure & Safe:** Jailbreak koruması, JSON format zorlama ve halüsinasyon önleyici BOS token yönetimi.
-   ✅ **Omni-Studio v3:** Entegre UI ile gerçek zamanlı test, "Reasoning" izleme ve donanım kontrolü.

---

## 🏗️ Mimari & Teknoloji

-   **Core:** `llama.cpp` (b7415+) üzerine kurulu özel C++ motoru.
-   **API:** gRPC (Stream + mTLS) ve HTTP/REST (OpenAI Uyumlu).
-   **Modeller:**
    -   **Production:** `gemma-3-4b-it` (Hız ve Tutarlılık için önerilir).
    -   **Research:** `qwen-2.5-7b-instruct` (Karmaşık mantık için).

### Performans Referansları (RTX 3060 6GB)

| Metrik | Gemma 3 4B | Qwen 2.5 7B |
|---|---|---|
| **TTFT (Gecikme)** | **~250ms** ⚡ | ~1200ms |
| **TPS (Hız)** | **~65 t/s** | ~35 t/s |
| **Bellek (VRAM)** | ~3.8 GB | ~5.2 GB |

---

## 🛠️ Kurulum ve Test

### 1. Başlatma
```bash
make up
```

### 2. Tam Kapsamlı Test (Matrix Validation)
Sistemin tüm yeteneklerini (Empati, Mantık, Format, Hız) test etmek için:
```bash
make test
```
*Bu komut, 2 farklı model üzerinde 14 farklı senaryoyu (toplam ~30 test adımı) otomatik olarak çalıştırır.*

---

## ⚙️ Yapılandırma (`.env`)

| Değişken | Önerilen | Açıklama |
|---|---|---|
| `LLM_LLAMA_SERVICE_GPU_LAYERS` | `100` | Tüm katmanlar GPU'da. |
| `LLM_LLAMA_SERVICE_CONTEXT_SIZE` | `4096` | Telefon görüşmesi için yeterli. |
| `LLM_LLAMA_SERVICE_KV_OFFLOAD` | `true` | Hız için kritik. |

---

## 📜 Lisans
Bu proje **AGPL-3.0** lisansı altındadır.

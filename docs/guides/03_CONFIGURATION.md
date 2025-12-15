# ⚙️ Yapılandırma Rehberi

Bu servis iki katmanlı bir yapılandırma kullanır:
1.  **Profil (`profiles.json`):** Modelin zekasını ve karakterini belirler.
2.  **Ortam (`.env`):** Donanım kaynaklarını ve ağ ayarlarını belirler.

---

### 1. Profil Seçimi

Varsayılan profil: `qwen25_3b_phone_assistant` (src/config.h içinde hardcoded).
Bu profil, **Telefon Asistanı** senaryosu için optimize edilmiştir:
-   Düşük `temperature` (0.2) ile halüsinasyon riskini azaltır.
-   Türkçe talimatlarla güçlendirilmiş System Prompt kullanır.

Gelecekte farklı bir profil seçmek için `LLM_LLAMA_SERVICE_PROFILE` değişkeni eklenecektir (Roadmap).

### 2. RAG Konfigürasyonu

RAG (Retrieval Augmented Generation) için özel bir ayar yapmanıza gerek yoktur.
Sistem, gRPC veya HTTP isteği içinde `rag_context` alanı dolu gelirse, otomatik olarak **"RAG Modu"**na geçer ve `PromptFormatter` şu şablonu uygular:

```text
### CONTEXT / BILGI NOTU:
[Gelen rag_context verisi]

### YONERGE:
Kullanicinin sorusunu SADECE yukaridaki Context bilgilerini kullanarak cevapla...
```

Bu yapı, Qwen 2.5 modelinin dış bilgiye sadık kalmasını garanti eder.

### 3. GPU Bellek Yönetimi (VRAM)

**Qwen 2.5 3B Modeli için Kaynak Tüketimi:**

| Ayar | Değer | VRAM Kullanımı (Yaklaşık) |
|---|---|---|
| Model Ağırlıkları | Q5_K_M | ~2.4 GB |
| KV Cache (Context) | 8192 Token | ~0.5 GB (Kullanıcı başına) |
| **Toplam (4 Kullanıcı)** | | **~4.5 GB** |

**Önemli:** 6GB VRAM'li bir kartta (RTX 3060/4050) güvenle 4 eşzamanlı çağrı yönetebilirsiniz. Daha fazlası için `LLM_LLAMA_SERVICE_CONTEXT_SIZE` değerini 4096'ya düşürün.
```
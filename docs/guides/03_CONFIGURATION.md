# ⚙️ Yapılandırma Rehberi

Bu servis iki katmanlı bir yapılandırma kullanır:
1.  **Profil (`models/profiles.json`):** Modelin zekasını, karakterini ve **prompt şablonlarını** belirler.
2.  **Ortam (`.env`):** Donanım kaynaklarını ve ağ ayarlarını belirler.

---

### 1. Profil ve Prompt Şablonları

Sistem, `models/profiles.json` dosyasındaki profillere göre çalışır. Her profil artık bir `templates` nesnesi içerir:

```json
"qwen3_4b_agent": {
    "display_name": "Sentiric Phone Agent (Qwen 3 4B)",
    "model_id": "ggml-org/Qwen3-4B-GGUF",
    // ...diğer ayarlar...
    "templates": {
        "system_prompt": "Sen yardımsever bir çağrı merkezi asistanısın...",
        "rag_prompt": "### BELGE:\\n{{rag_context}}\\n\\n### SORU:\\n{{user_prompt}}\\n\\n### YÖNERGE:\\nYukarıdaki soruyu SADECE yukarıdaki BELGE'yi kullanarak cevapla."
    }
}
```

-   `system_prompt`: Varsayılan sistem talimatı.
-   `rag_prompt`: **(KRİTİK)** Bir RAG isteği geldiğinde kullanılacak tam görev şablonu.
    -   `{{rag_context}}`: Gelen RAG verisi bu yer tutucu ile değiştirilir.
    -   `{{user_prompt}}`: Kullanıcının asıl sorusu bu yer tutucu ile değiştirilir.
    -   Bu yapı, her modelin en iyi performans gösterdiği RAG formatını C++ kodunu değiştirmeden tanımlamanızı sağlar. Şablon, modeli SADECE verilen bağlamı kullanmaya zorlayacak şekilde tasarlanmalıdır.

### 2. RAG Konfigürasyonu

RAG (Retrieval Augmented Generation) için özel bir C++ ayarı yapmanıza gerek yoktur.
Sistem, gRPC veya HTTP isteği içinde `rag_context` alanı dolu gelirse, otomatik olarak aktif profilin `rag_prompt` şablonunu kullanır ve her iki yer tutucuyu da doldurur. Eğer `rag_context` boş ise, standart `system_prompt` ile normal bir konuşma akışı kullanılır.

### 3. GPU Bellek Yönetimi (VRAM)

**Qwen 2.5 3B Modeli için Kaynak Tüketimi:**

| Ayar | Değer | VRAM Kullanımı (Yaklaşık) |
|---|---|---|
| Model Ağırlıkları | Q5_K_M | ~2.4 GB |
| KV Cache (Context) | 8192 Token | ~0.5 GB (Kullanıcı başına) |
| **Toplam (4 Kullanıcı)** | | **~4.5 GB** |

**Önemli:** 6GB VRAM'li bir kartta (RTX 3060/4050) güvenle 4 eşzamanlı çağrı yönetebilirsiniz. Daha fazlası için `.env` dosyasında `LLM_LLAMA_SERVICE_CONTEXT_SIZE` değerini 4096'ya düşürün.


---

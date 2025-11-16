# 🚀 Bellek Yönetimi ve Performans Ayarları

`sentiric-llm-llama-service`, kısıtlı donanım kaynaklarından bile maksimum verim alacak şekilde tasarlanmıştır. Ancak bu, doğru yapılandırma ile mümkündür. Bu belge, servisin performansını ve bellek kullanımını nasıl yöneteceğinizi açıklar.

## Temel Bellek Formülü

Bir GPU üzerinde çalışırken, toplam VRAM kullanımını şu basit formül belirler:

**Toplam VRAM ≈ Model VRAM + ( `THREADS` × Context Başına VRAM )**

-   **Model VRAM:** Modelin GPU'ya yüklenen katmanlarının (`LLM_LLAMA_SERVICE_GPU_LAYERS`) kapladığı alan.
-   **THREADS:** Aynı anda işlenebilecek istek sayısı (`LLM_LLAMA_SERVICE_THREADS`).
-   **Context Başına VRAM:** Her bir `context`'in KV cache'i için ayırdığı bellek. Bu, `LLM_LLAMA_SERVICE_CONTEXT_SIZE` ile doğru orantılıdır.

Eğer bu toplam, GPU'nuzun toplam VRAM'ini aşarsa, `cudaMalloc failed: out of memory` hatası alırsınız.

### Örnek Senaryo: 6GB VRAM'li Bir GPU

-   **Model:** `gemma-3-4b-it-Q4_K_M.gguf` (Yaklaşık **2.8 GB** VRAM kullanır).
-   **Kalan VRAM:** 6 GB - 2.8 GB = **3.2 GB**

| `CONTEXT_SIZE` | Context Başına VRAM (Tahmini) | `THREADS` | Toplam KV Cache VRAM | Toplam VRAM | Sonuç |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `8192` | ~1.6 GB | `2` | 3.2 GB | 2.8 + 3.2 = **6.0 GB** | **Riskli** ❌ |
| `8192` | ~1.6 GB | `1` | 1.6 GB | 2.8 + 1.6 = **4.4 GB** | **Güvenli** ✅ |
| `4096` | ~0.8 GB | `3` | 2.4 GB | 2.8 + 2.4 = **5.2 GB** | **Güvenli** ✅ |
| `4096` | ~0.8 GB | `2` | 1.6 GB | 2.8 + 1.6 = **4.4 GB** | **Çok Güvenli** ✅ |

**Kural:** Bellek hatası alıyorsanız, ilk olarak `LLM_LLAMA_SERVICE_THREADS` veya `LLM_LLAMA_SERVICE_CONTEXT_SIZE` değerlerini düşürün.

## 1. Bellek Haritalama (Memory Mapping - mmap)

-   **Ne İşe Yarar?** Normalde, bir LLM servisi çalıştığında, model dosyasının (GB'larca büyüklükte olabilir) tamamını RAM'e yükler. `mmap` etkinleştirildiğinde (`LLM_LLAMA_SERVICE_USE_MMAP=true`), servis modeli RAM'e kopyalamak yerine, işletim sisteminin sanal bellek yöneticisini kullanarak doğrudan disk üzerindeki dosyaya erişir.
-   **Avantajı:** RAM kullanımı dramatik şekilde düşer. Özellikle RAM'i model boyutundan daha az olan sistemlerde servisin çalışabilmesini sağlar ve başlangıç süresini kısaltır.

## 2. KV Cache GPU Offloading

-   **Ne İşe Yarar?** Bir LLM, metin üretirken daha önce ürettiği token'ları "hatırlamak" için bir KV cache (anahtar-değer önbelleği) kullanır. Bu cache, context boyutu büyüdükçe çok fazla bellek tüketir. Bu ayar (`LLM_LLAMA_SERVICE_KV_OFFLOAD=true`) etkinleştirildiğinde, bu cache'in tamamı CPU'nun RAM'i yerine GPU'nun VRAM'ine yüklenir.
-   **Avantajı:**
    1.  Değerli sistem RAM'ini serbest bırakır.
    2.  Token üretimi sırasında CPU ve GPU arasında veri transferini ortadan kaldırarak **üretim hızını (token/saniye) ciddi şekilde artırır.**

## 3. Akıllı Bağlam Kaydırma (Context Shifting)

Bu, servisin en güçlü yeteneklerinden biridir.

-   **Problem:** Eğer modele, `CONTEXT_SIZE` (örn: 8192 token) ayarından daha büyük bir metin (örn: 20,000 token'lık bir RAG dokümanı) verirseniz ne olur? Çoğu servis hata verir veya çöker.
-   **Çözüm:** `llm-llama-service`, bu durumu akıllıca yönetir.
    1.  Prompt'un, context penceresine sığmayacak kadar büyük olduğunu anlar.
    2.  Prompt'u tek seferde işlemek yerine, bir döngü içinde parçalar halinde işlemeye başlar.
    3.  Modelin hafızası (KV cache) dolduğunda, en eski bilgileri (prompt'un başlangıcı) hafızadan atar ve yeni bilgilere yer açar. Bunu `llama_kv_cache_seq_rm` fonksiyonu ile yapar.
-   **Avantajı:** Bu mekanizma sayesinde servis, teorik olarak **sonsuz uzunluktaki metinleri bile** sınırlı bir bellek (VRAM/RAM) ile işleyebilir. Bu, çok büyük dokümanlar üzerinde RAG analizi yapmak için servisi ideal hale getirir.

---
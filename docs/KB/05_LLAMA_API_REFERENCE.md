# 💡 KB-05: llama.cpp API Hızlı Referans Kılavuzu

**AMAÇ:** Bu doküman, `sentiric-llm-llama-service` projesinde sıkça kullanılan `llama.cpp` API fonksiyonlarının kategorize edilmiş bir listesini sunar. Geliştirme sırasında hızlı bir başvuru kaynağı olarak tasarlanmıştır. API'nin bağlayıcı kontratı ve spesifik kullanım desenleri için her zaman **[KB-04: Proje İçi API Bağlayıcı Kontratı](./04_LLAMA_CPP_API_BINDING.md)** belgesine başvurulmalıdır.

---

### Kategori 1: Başlatma ve Yönetim (Initialization & Management)

| Fonksiyon | Açıklama |
| :--- | :--- |
| `llama_backend_init()` | `llama.cpp` altyapısını program başında bir kez başlatır. |
| `llama_model_load_from_file()` | Bir `.gguf` model dosyasını diskten yükler ve bir model nesnesi oluşturur. |
| `llama_init_from_model()` | Yüklenmiş bir modelden, asıl çıkarım işlemlerinin yapılacağı `context`'i oluşturur. |
| `llama_model_free()` | Model tarafından kullanılan belleği serbest bırakır. |
| `llama_free()` | Context tarafından kullanılan belleği serbest bırakır. |
| `llama_backend_free()` | Program sonunda altyapıyı kapatır. |

### Kategori 2: Bellek ve KV Cache Yönetimi

| Fonksiyon | Açıklama |
| :--- | :--- |
| **`llama_get_memory(ctx)`** | Bir context'in bellek yöneticisi nesnesini (`llama_memory_t`) döndürür. |
| **`llama_memory_seq_rm()`** | **(KRİTİK)** Belirtilen bir sequence'in KV cache'inin bir kısmını veya tamamını temizler. "Context Shifting" ve havuz temizliği için bu kullanılır. |
| **`llama_memory_seq_add()`** | Temizlenmiş bir KV cache'e pozisyonel bir ofset ekler. "Context Shifting" için gereklidir. |
| `llama_memory_seq_cp()` | Bir sequence'in KV cache'ini başka bir sequence'e kopyalar (İleri seviye dallanma/forking senaryoları için). |

### Kategori 3: Çıkarım (Inference)

| Fonksiyon | Açıklama |
| :--- | :--- |
| `llama_batch_init()` | Token'ları ve pozisyonlarını içeren bir "batch" (iş yığını) nesnesi oluşturur. |
| `llama_decode(ctx, batch)` | Verilen "batch"i işler, modelin durumunu günceller ve logit'leri (bir sonraki token tahminleri) hesaplar. |
| `llama_get_logits_ith(ctx, i)` | Son `llama_decode` çağrısından sonra, belirtilen `i`. pozisyondaki token için logit'leri döndürür. |
| `llama_batch_free()` | Oluşturulan "batch" nesnesini serbest bırakır. |

### Kategori 4: Tokenizasyon (Tokenization)

| Fonksiyon | Açıklama |
| :--- | :--- |
| `llama_model_get_vocab()` | Modelin kelime dağarcığı (`vocab`) nesnesini döndürür. |
| `llama_tokenize()` | Bir metin parçasını (`char*`) bir token dizisine (`llama_token[]`) çevirir. |
| `llama_token_to_piece()` | Tek bir token'ı, okunabilir bir metin parçasına (`char*`) çevirir. |

### Kategori 5: Örnekleme (Sampling)

| Fonksiyon | Açıklama |
| :--- | :--- |
| `llama_sampler_chain_init()` | Birden fazla örnekleme kuralını (top-k, top-p, sıcaklık vb.) bir araya getiren bir zincir oluşturur. |
| `llama_sampler_chain_add()` | Örnekleme zincirine yeni bir kural ekler (örn: `llama_sampler_init_top_k()`). |
| `llama_sampler_sample()` | Verilen logit'lere, zincirdeki tüm kuralları uygulayarak nihai bir token seçer. |
| `llama_sampler_accept()` | Seçilen token'ı zincire "kabul ettirir". Bu, tekrarlama cezası gibi durumları günceller. |
| `llama_sampler_free()` | Örnekleme zincirini bellekten temizler. |

---

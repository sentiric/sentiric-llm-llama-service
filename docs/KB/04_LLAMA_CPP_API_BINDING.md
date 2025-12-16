# 💡 KB-04: Proje İçi `llama.cpp` API Bağlayıcı Kontratı

**AMAÇ:** Bu doküman, projenin kullandığı `llama.cpp` versiyonuna (b7415) özel API kullanım desenlerini ve bellek yönetimi kurallarını tanımlayan **bağlayıcı teknik şartnamedir**. `LLMEngine` ve `Service` katmanları bu kurallara sıkı sıkıya uymalıdır.

## 1. Bağlı Olunan Versiyon ve Kaynaklar

-   **Versiyon Tag:** `b7415` (master)
-   **Referans Commit:** ``
-   **Header Dosyaları:**
    -   `include/llama.h`: Çekirdek API.
    -   `common/common.h`: Yardımcı araçlar (Batch ekleme, string işleme vb.).

**KURAL:** `llama.h` içindeki fonksiyon imzaları esastır. `llama_token_...` fonksiyonlarının çoğu `deprecated` olmuş, yerini `llama_vocab_...` fonksiyonlarına bırakmıştır.

---

## 2. Başlatma (Initialization) Akışı

Eski `llama_new_context_with_model` yapısı **deprecated** olmuştur. Model ve Context yükleme yaşam döngüsü ayrılmıştır.

```cpp
// 1. Backend Başlatma (Program başında 1 kez)
llama_backend_init();

// 2. Model Yükleme
auto mparams = llama_model_default_params();
mparams.n_gpu_layers = 99; // GPU offload
struct llama_model* model = llama_model_load_from_file("model.gguf", mparams);

if (!model) { /* Hata Yönetimi */ }

// 3. Context (Oturum) Oluşturma
auto cparams = llama_context_default_params();
cparams.n_ctx = 4096;
cparams.n_batch = 512;
cparams.flash_attn_type = LLAMA_FLASH_ATTN_TYPE_ENABLED; // Flash Attention desteği

struct llama_context* ctx = llama_init_from_model(model, cparams);

if (!ctx) { /* Hata Yönetimi */ }

// 4. Vocab (Kelime Dağarcığı) Erişimi (ÖNEMLİ: Token işlemleri için gerekli)
const struct llama_vocab* vocab = llama_model_get_vocab(model);
```

---

## 3. Tokenizasyon ve Detokenizasyon

**KRİTİK DEĞİŞİKLİK:** Artık tokenizasyon işlemleri `llama_context` değil, `llama_vocab` üzerinden yapılmaktadır.

```cpp
// Metni Token'a Çevirme
std::string text = "Merhaba dünya";
int n_tokens_max = text.length() + 2;
std::vector<llama_token> tokens(n_tokens_max);

// NOT: llama_tokenize artık 'vocab' alır
int n_tokens = llama_tokenize(vocab, text.c_str(), text.length(), tokens.data(), n_tokens_max, true, false);
if (n_tokens < 0) { /* Buffer yetersiz hatası */ }
tokens.resize(n_tokens);

// Token'ı Metne Çevirme
char buf[256];
// NOT: llama_token_to_piece artık 'vocab' alır
int n = llama_token_to_piece(vocab, tokens[0], buf, sizeof(buf), 0, true);
std::string piece(buf, n);
```

---

## 4. Inference (Çıkarım) Döngüsü ve Batch Yönetimi

Batch yönetimi için `llama_batch` struct'ı ve `common.h` içindeki yardımcı fonksiyon kullanılmalıdır.

```cpp
// Batch Hazırlama
llama_batch batch = llama_batch_init(512, 0, 1); // n_tokens, embd, n_seq_max

// Prompt'u Batch'e Ekleme (common.h yardımcısı ile)
for (size_t i = 0; i < prompt_tokens.size(); ++i) {
    // Son token hariç logits hesaplama (false), son token için hesapla (true)
    bool calc_logits = (i == prompt_tokens.size() - 1);
    common_batch_add(batch, prompt_tokens[i], i, {0}, calc_logits);
}

// Decode İşlemi
if (llama_decode(ctx, batch) != 0) {
    // 1 = KV Cache dolu, <0 = Hata
    /* Hata Yönetimi */
}

// Sonraki döngüler için batch temizliği
// llama_batch_free(batch); // Sadece iş tamamen bitince
common_batch_clear(batch); // Döngü içinde yeniden kullanım için
```

---

## 5. Örnekleme (Sampling) Zinciri

`llama.cpp` b7415, modüler bir `llama_sampler` yapısı kullanır. En sonda mutlaka bir seçim yapıcı (Greedy veya Dist) olmalıdır.

```cpp
// Zincir parametreleri
auto sparams = llama_sampler_chain_default_params();
llama_sampler* chain = llama_sampler_chain_init(sparams);

// Zincire Kurallar Ekleme (Sıralama Önemli)
llama_sampler_chain_add(chain, llama_sampler_init_penalties(
    64,     // last_n
    1.1f,   // repeat penalty
    0.0f,   // frequency penalty
    0.0f    // presence penalty
));
llama_sampler_chain_add(chain, llama_sampler_init_top_k(40));
llama_sampler_chain_add(chain, llama_sampler_init_top_p(0.95f, 1));
llama_sampler_chain_add(chain, llama_sampler_init_temp(0.8f));

// KRİTİK: Zincirin sonunda mutlaka bir seçim sampler'ı olmalı
// llama_sampler_init_greedy() veya llama_sampler_init_dist(seed)
llama_sampler_chain_add(chain, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));

// Token Seçimi
llama_token new_token_id = llama_sampler_sample(chain, ctx, -1); // -1: son token
llama_sampler_accept(chain, new_token_id); // Seçimi kabul et (state güncelle)

// Temizlik
llama_sampler_free(chain);
```

---

## 6. Bellek ve KV Cache Yönetimi (KRİTİK)

`llama.cpp` modern API'sinde KV Cache manipülasyonu `llama_context` üzerinden değil, `llama_get_memory(ctx)` ile alınan `llama_memory_t` arayüzü üzerinden yapılır.

**Yanlış Kullanım:** `llama_kv_cache_seq_rm(ctx, ...)` (Artık yok veya deprecated)
**Doğru Kullanım:**

```cpp
// 1. Bellek Yöneticisini Al
llama_memory_t mem = llama_get_memory(ctx);

// 2. Bir Sequence'i Tamamen Silme (Context havuza iade edilirken)
llama_memory_seq_rm(mem, 0, -1, -1); 
// seq_id=0, p0=-1 (baştan), p1=-1 (sona kadar)

// 3. Context Shifting (Kaydırma)
// Örn: İlk 10 tokenı sil, kalanları başa kaydır
llama_memory_seq_rm(mem, 0, 0, 10);      // [0, 10) aralığını sil
llama_memory_seq_add(mem, 0, 10, -1, -10); // [10, son) aralığını -10 pozisyon kaydır
```

---

## 7. Temizlik (Teardown)

Sıralama önemlidir.

```cpp
llama_batch_free(batch);
llama_sampler_free(chain);
llama_free(ctx);
llama_model_free(model);
llama_backend_free();
```

---


## 8. LoRA Adaptör Yönetimi (YENİ)

LoRA adaptörleri **Model** seviyesinde yüklenir, ancak **Context** seviyesinde aktif edilir. Bu, aynı modeli kullanan farklı context'lerin farklı LoRA ayarlarına sahip olabilmesini sağlar.

```cpp
// 1. LoRA Adaptörünü Yükleme (Dosyadan)
// Adaptör model ile ilişkilendirilir.
struct llama_adapter_lora* lora_adapter = llama_adapter_lora_init(model, "path/to/adapter.gguf");

if (!lora_adapter) { /* Hata Yönetimi: Dosya bulunamadı veya uyumsuz */ }

// 2. Adaptörü Context'e Uygulama
// scale: Etki oranı (0.0 - 1.0 arası veya daha yüksek). 
// Birden fazla LoRA eklenebilir.
float scale = 0.8f;
int32_t err = llama_set_adapter_lora(ctx, lora_adapter, scale);

if (err != 0) { /* Hata Yönetimi */ }

// 3. Adaptörü Context'ten Kaldırma (İsteğe bağlı)
// Sadece bu context üzerindeki etkiyi kaldırır, adaptör modelde yüklü kalır.
llama_rm_adapter_lora(ctx, lora_adapter);

// 4. Tüm Adaptörleri Temizleme
llama_clear_adapter_lora(ctx);

// 5. Adaptörü Bellekten Silme
// DİKKAT: `llama_model_free(model)` çağrıldığında bağlı adaptörler otomatik silinir.
// Ancak manuel olarak erkenden silmek isterseniz:
llama_adapter_lora_free(lora_adapter);
```

**KURAL:** LoRA adaptörleri `llama_decode` çağrısından **önce** set edilmelidir. Decode işlemi sırasında dinamik olarak değiştirmek mümkündür ancak KV Cache üzerindeki etkileri (cache'in yeniden hesaplanması gerekip gerekmediği) dikkate alınmalıdır. Genellikle session başında set edilmesi önerilir.

---

## 9. Temizlik (Teardown)

Sıralama önemlidir. Model silindiğinde bağlı LoRA'lar da silinir.

```cpp
llama_batch_free(batch);
llama_sampler_free(chain);
// Context silindiğinde üzerindeki LoRA bağları kopar (adaptör silinmez)
llama_free(ctx); 
// Model silindiğinde bağlı LoRA adaptörleri (llama_adapter_lora_init ile gelenler) serbest bırakılır
llama_model_free(model);
llama_backend_free();
```


---

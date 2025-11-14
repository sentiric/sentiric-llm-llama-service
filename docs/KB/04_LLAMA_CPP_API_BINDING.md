# 💡 KB-04: Proje İçi `llama.cpp` API Bağlayıcı Kontratı

**AMAÇ:** Bu doküman, bu projenin kullandığı `llama.cpp` versiyonuna özel API kullanım desenlerini tanımlayan **tek ve mutlak doğru kaynaktır**. `LLMEngine` üzerinde geliştirme yaparken başka hiçbir varsayımda bulunulmamalıdır. Bir API hatası alındığında, ilk olarak buraya başvurulmalı, eğer bilgi eksikse, `llama.cpp` kaynak kodu incelenerek **önce bu belge güncellenmeli, sonra kod düzeltilmelidir.**

## 1. Bağlı Olunan Versiyon

-   **Commit Hash:** `92bb442ad999a0d52df0af2730cd861012e8ac5c`
-   **Commit Linki:** [https://github.com/ggml-org/llama.cpp/commit/92bb442ad999a0d52df0af2730cd861012e8ac5c](https://github.com/ggml-org/llama.cpp/commit/92bb442ad999a0d52df0af2730cd861012e8ac5c)
-   **Referans `llama.h`:** [llama.h @ 92bb442](https://github.com/ggml-org/llama.cpp/blob/92bb442ad999a0d52df0af2730cd861012e8ac5c/include/llama.h)
-   **Referans `common.h`:** [common.h @ 92bb442](https://github.com/ggml-org/llama.cpp/blob/92bb442ad999a0d52df0af2730cd861012e8ac5c/common/common.h)
-   **Son Doğrulama Tarihi:** 2025-11-13

**KURAL:** `Dockerfile`'daki `LLAMA_CPP_VERSION` değiştirilirse, bu belge **mutlaka** güncellenmelidir.

---

## 2. Temel API Kullanım Desenleri

### 2.1. Başlatma ve Sonlandırma Sırası

```cpp
// 1. Backend'i başlat (program başına bir kez)
llama_backend_init();

// 2. Model parametrelerini ayarla ve modeli yükle
llama_model_params model_params = llama_model_default_params();
llama_model* model = llama_model_load_from_file(model_path.c_str(), model_params);

// 3. Context parametrelerini ayarla ve context'i oluştur
llama_context_params ctx_params = llama_context_default_params();
ctx_params.n_ctx = 4096;
llama_context* ctx = llama_init_from_model(model, ctx_params);

// ... kullanım ...

// Temizlik Sırası
llama_free(ctx);
llama_model_free(model);
llama_backend_free();
```

### 2.2. Token Üretim (Inference) Döngüsü

Bu desen, `LlamaContextPool`'dan kiralanan bir `llama_context` üzerinde çalışır.

```cpp
// --- Gerekli Başlık Dosyaları ---
#include "llama.h"
#include "common.h" // common_batch_add için

// --- 1. Prompt'u İşleme ---

const auto* vocab = llama_model_get_vocab(model);
std::vector<llama_token> prompt_tokens;
// ... llama_tokenize(vocab, ...) çağrısı ...
prompt_tokens.resize(n_tokens);

llama_batch batch = llama_batch_init(n_tokens, 0, 1);
for (int i = 0; i < n_tokens; ++i) {
    // DOĞRU KULLANIM: `common.h`'dan gelen yardımcı fonksiyonu kullan.
    common_batch_add(batch, prompt_tokens[i], i, {0}, false);
}
batch.logits[batch.n_tokens - 1] = true;

if (llama_decode(ctx, batch) != 0) { /* Hata yönetimi */ }

// --- 2. Yeni Token'ları Üretme Döngüsü ---

llama_pos n_past = batch.n_tokens;
while (n_past < n_ctx) {
    llama_token new_token_id = llama_sampler_sample(sampler_chain, ctx, -1);
    llama_sampler_accept(sampler_chain, new_token_id);
    if (llama_vocab_is_eog(vocab, new_token_id)) break;

    // ... token'ı metne çevir ...

    llama_batch_free(batch);
    batch = llama_batch_init(1, 0, 1);
    common_batch_add(batch, new_token_id, n_past, {0}, true);

    if (llama_decode(ctx, batch) != 0) { /* Hata yönetimi */ }
    n_past++;
}

llama_batch_free(batch);
```

### 2.3. Örnekleme (Sampling) Zinciri Oluşturma

`llama_sampler_init_penalties` fonksiyonu `llama_context*` **almaz**.

```cpp
// DOĞRU KULLANIM:
llama_sampler_chain_params sparams = llama_sampler_chain_default_params();
llama_sampler* sampler_chain = llama_sampler_chain_init(sparams);

// İlk parametre `penalty_last_n` (int32_t), context boyutu olabilir.
llama_sampler_chain_add(sampler_chain, llama_sampler_init_penalties(
    llama_n_ctx(ctx), // penalty_last_n
    1.1f,             // penalty_repeat
    0.0f,             // penalty_freq
    0.0f              // penalty_present
));
llama_sampler_chain_add(sampler_chain, llama_sampler_init_top_k(40));
// ... diğer sampler'lar ...

// YANLIŞ KULLANIM:
// llama_sampler_chain_add(sampler_chain, llama_sampler_init_penalties(ctx, ...)); // DERLEME HATASI
```

### 2.4. KV Cache Yönetimi (KRİTİK)

Bir `llama_context` havuza iade edilmeden önce, içindeki tüm KV cache verileri temizlenmelidir.

**DOĞRU YÖNTEM:**
    common.cpp dosyası, common kütüphanesine değil, doğrudan ana llama kütüphanesine derleniyor. Bu nedenle -lcommon linkleme hatası veriyor.

    llama.h dosyasında llama_kv_cache_seq_rm veya llama_kv_cache_clear bulunmuyor. İncelediğin örneklerde (server.cpp, main.cpp vb.) bu işlem llama_memory_seq_rm(llama_get_memory(ctx), ...) veya llama_kv_cache_clear(ctx) (eski versiyonlar) veya bazen llama_memory_clear(llama_get_memory(ctx), true) ile yapılıyor. En güvenilir ve modern yöntem, tüm dizinleri (seq_id = -1) ve pozisyonları (p0 = -1, p1 = -1) hedef alan llama_memory_seq_rm çağrısıdır.
```cpp
// Bir context'i havuza iade etmeden hemen önce 
llama_memory_seq_rm(llama_get_memory(ctx), -1, -1, -1) olarak düzelt ve llama_get_memory(ctx) çağrısının zorunlu olduğunu belirt.
```

---

## 3. Derleme ve Linkleme Notları

- **`common` Kütüphanesi:** `llama.cpp` projesi, `common` adında ayrı bir linklenebilir kütüphane **oluşturmaz**. `common` dizinindeki kaynak dosyalar, ana `llama` kütüphanesinin içine derlenir. Bu nedenle, `target_link_libraries` komutunda `common`'a linkleme yapılmamalıdır.
- **Başlık Dosyaları:** `common.h` gibi yardımcı başlık dosyalarını kullanabilmek için `llama.cpp/common` dizini `target_include_directories` ile projemize dahil edilmelidir.

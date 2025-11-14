# 💡 KB-04: Proje İçi `llama.cpp` API Bağlayıcı Kontratı

**AMAÇ:** Bu doküman, bu projenin kullandığı `llama.cpp` versiyonuna özel API kullanım desenlerini tanımlayan **tek ve mutlak doğru kaynaktır**. `LLMEngine` üzerinde geliştirme yaparken başka hiçbir varsayımda bulunulmamalıdır. Bir API hatası alındığında, ilk olarak buraya başvurulmalı, eğer bilgi eksikse, `llama.cpp` kaynak kodu incelenerek **önce bu belge güncellenmeli, sonra kod düzeltilmelidir.**

## 1. Bağlı Olunan Versiyon

-   **Commit Hash:** `92bb442ad999a0d52df0af2730cd861012e8ac5c`
-   **Commit Linki:** [https://github.com/ggml-org/llama.cpp/commit/92bb442ad999a0d52df0af2730cd861012e8ac5c](https://github.com/ggml-org/llama.cpp/commit/92bb442ad999a0d52df0af2730cd861012e8ac5c)
-   **Referans `llama.h`:** [llama.h @ 92bb442](https://github.com/ggml-org/llama.cpp/blob/92bb442ad999a0d52df0af2730cd861012e8ac5c/include/llama.h)
-   **Referans `common.h`:** [common.h @ 92bb442](https://github.com/ggml-org/llama.cpp/blob/92bb442ad999a0d52df0af2730cd861012e8ac5c/common/common.h)
-   **Son Doğrulama Tarihi:** 2025-11-14

**KURAL:** `Dockerfile`'daki `LLAMA_CPP_VERSION` değiştirilirse, bu belge **mutlaka** güncellenmelidir.

---

## 2. Derleme ve Linkleme (CMake)

-   **`common` Kütüphanesi:** `llama.cpp`, yardımcı fonksiyonları içeren `common` adında statik bir kütüphane oluşturur. Ancak bu, `LLAMA_BUILD_COMMON` CMake seçeneği `ON` olarak ayarlandığında gerçekleşir. Bizim projemiz, `add_subdirectory` çağırmadan önce bu seçeneği `ON` olarak ayarlayarak `common` kütüphanesinin derlenmesini garanti eder.
-   **Linkleme:** `llm_service` ve `llm_cli` hedefleri, `target_link_libraries` aracılığıyla hem `llama` hem de `common` hedeflerine linklenmelidir.
-   **Başlık Dosyaları:** `llama.h` ve `common.h` dosyalarını kullanabilmek için `llama.cpp/include` ve `llama.cpp/common` dizinleri `target_include_directories` ile projeye dahil edilmelidir.

---

## 3. Temel API Kullanım Desenleri

### 3.1. Token Üretim (Inference) Döngüsü

Bu desen, `LlamaContextPool`'dan kiralanan bir `llama_context` üzerinde çalışır.

```cpp
// --- Gerekli Başlık Dosyaları ---
#include "llama.h"
#include "common.h"

// --- 1. Prompt'u İşleme ---

const auto* vocab = llama_model_get_vocab(model);
std::vector<llama_token> prompt_tokens;
// ... llama_tokenize(vocab, ...) çağrısı ile tokenleri al ...
prompt_tokens.resize(n_tokens);

llama_batch batch = llama_batch_init(n_tokens, 0, 1);
for (int i = 0; i < n_tokens; ++i) {
    // DOĞRU KULLANIM: `common.h`'dan gelen yardımcı fonksiyonu kullan.
    common_batch_add(batch, prompt_tokens[i], i, {0}, false);
}
batch.logits[batch.n_tokens - 1] = true; // Sadece son token'ın logit'lerine ihtiyacımız var

if (llama_decode(ctx, batch) != 0) { /* Hata yönetimi */ }

// --- 2. Yeni Token'ları Üretme Döngüsü ---

llama_pos n_past = batch.n_tokens;
while (n_past < n_ctx) {
    llama_token new_token_id = llama_sampler_sample(sampler_chain, ctx, -1);
    llama_sampler_accept(sampler_chain, new_token_id);
    if (llama_vocab_is_eog(vocab, new_token_id)) break;

    // ... token'ı metne çevir (`llama_token_to_piece`) ...

    llama_batch_free(batch); // Önceki batch'i temizle
    batch = llama_batch_init(1, 0, 1);
    common_batch_add(batch, new_token_id, n_past, {0}, true);

    if (llama_decode(ctx, batch) != 0) { /* Hata yönetimi */ }
    n_past++;
}

llama_batch_free(batch);
```

### 3.2. Örnekleme (Sampling) Zinciri Oluşturma

`llama_sampler_init_penalties` fonksiyonu `llama_context*` **almaz**.

```cpp
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
```

### 3.3. KV Cache Yönetimi (KRİTİK)

Bir `llama_context`, `LlamaContextPool`'a iade edilmeden önce, içindeki tüm KV cache verileri temizlenmelidir.

**DOĞRU YÖNTEM:**
```cpp
// Bir context'i havuza iade etmeden hemen önce çağrılacak kod:
// `llama_get_memory` ile context'in bellek yöneticisini al.
// seq_id = -1 -> Tüm dizinler
// p0 = -1, p1 = -1 -> Tüm pozisyonlar (veya p0=0, p1=-1)
llama_memory_seq_rm(llama_get_memory(ctx), -1, -1, -1);
```

---
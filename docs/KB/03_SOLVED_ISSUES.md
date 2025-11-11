# 💡 Çözülmüş Sorunlar Veritabanı (Knowledge Base)

Bu doküman, geliştirme sırasında karşılaşılan önemli sorunları, kök nedenlerini ve çözümlerini "post-mortem" formatında belgeler.

---

### SORUN-001: API Uyumsuzluğu Nedeniyle Derleme Hataları

-   **Tarih:** 2025-11-11
-   **Belirtiler:** `llama_kv_cache_clear`, `llama_sample_token_greedy`, `llama_batch_add` gibi fonksiyonların derleyici tarafından bulunamaması. `llama_tokenize` gibi fonksiyonların yanlış argüman aldığını belirten hatalar.
-   **Kök Neden:** Proje kodu, `llama.cpp` kütüphanesinin eski bir API setini kullanıyordu. Ancak `Dockerfile`, `git clone` ile kütüphanenin en güncel sürümünü çekiyordu. Bu, API imzaları ve fonksiyon adları arasında büyük bir uyumsuzluğa yol açtı.
-   **Çözüm:**
    1.  `llm_engine.cpp` ve `.h` dosyaları, güncel `llama.cpp` API'sine göre tamamen yeniden yazıldı (refactor edildi).
    2.  Model yükleme `llama_model_load_from_file` ile, context oluşturma `llama_init_from_model` ile güncellendi.
    3.  Token üretimi, `llama_sampler` (örnekleyici zinciri) mimarisine geçirildi. `llama_sampler_sample` ve `llama_sampler_accept` fonksiyonları kullanıldı.
    4.  Eski manuel KV cache yönetimi, `llama_memory_seq_rm` ile değiştirildi.

---

### SORUN-002: `libllama.so` / `libggml.so` Yüklenememe Hatası (Runtime)

-   **Tarih:** 2025-11-11
-   **Belirtiler:** Konteyner başlarken `error while loading shared libraries: libXXX.so: cannot open shared object file` hatası vererek çöküyordu.
-   **Kök Neden:** Docker multi-stage build sürecinde, `llama.cpp` tarafından derlenen paylaşılan kütüphaneler (`libllama.so`, `libggml.so` vb.) `builder` aşamasında kalıyor ve son `runtime` imajına kopyalanmıyordu.
-   **Çözüm:** `Dockerfile`'ın `runtime` aşamasına şu adımlar eklendi:
    1.  `builder` aşamasındaki `build/bin/` dizininde bulunan tüm `.so` dosyalarını (`*.so`) `runtime` aşamasındaki `/usr/local/lib/` dizinine kopyalayan bir `COPY` komutu eklendi.
    2.  İşletim sisteminin bu yeni kütüphaneleri tanıması için `RUN ldconfig` komutu eklendi.

---

### SORUN-003: `spdlog`/`fmt` Derleme Hatası (`Cannot format an argument`)

-   **Tarih:** 2025-11-11
-   **Belirtiler:** `static assertion failed: Cannot format an argument. To make type T formattable provide a formatter<T> specialization...` hatası.
-   **Kök Neden:** `spdlog` kütüphanesi, `grpc::StatusCode` gibi özel bir `enum` tipini nasıl string'e formatlayacağını bilmiyordu.
-   **Çözüm:** `src/cli/grpc_client.cpp` dosyasının başına, `grpc::StatusCode` için bir `fmt::formatter` uzmanlaşması (specialization) eklendi. Bu yapı, `fmt` kütüphanesine `grpc::StatusCode` enum'unu gördüğünde onu anlamlı bir metne (ör: "CANCELLED (1)") nasıl çevireceğini öğretti.

---


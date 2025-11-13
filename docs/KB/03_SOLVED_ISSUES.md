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

### SORUN-004: `llama.cpp`'nin Eski Versiyonu (`b7046`) ile Ciddi API Uyumsuzlukları

-   **Tarih:** 2025-11-13
-   **Belirtiler:** `llama_eval`, `llama_kv_cache_clear`, `llama_sample_top_k` gibi temel fonksiyonların derleyici tarafından bulunamaması (`was not declared in this scope`). `llama_tokenize` gibi fonksiyonların yanlış imza (`cannot convert 'llama_model*' to 'const llama_vocab*'`) ile çağrıldığını belirten hatalar.
-   **Kök Neden:** Proje kodu, `llama.cpp` kütüphanesinin modern API konseptlerine (örn: `llama_sampler_chain`, `vocab*` soyutlaması) dayalı olarak yazılmıştı. Ancak `Dockerfile` ile sabitlenen `b7046` commit'i, bu modern soyutlamaların **hiçbirini** içermeyen çok daha eski ve temel bir API setine sahiptir. Temel hata, `b7046` API'sinin yetenekleri ve yapısı hakkındaki yanlış varsayımlardı. Özellikle, örnekleme (sampling) fonksiyonlarının `libllama.so`'nun bir parçası olmadığı, `examples/` dizinindeki yardımcı kodlar olduğu anlaşıldı.
-   **Çözüm:**
    1.  **Tam Geri Çekilme:** `LLMEngine`, `b7046`'nın `include/llama.h` dosyasında `LLAMA_API` ile işaretlenmiş **sadece** temel ve halka açık fonksiyonları kullanacak şekilde sıfırdan yeniden yazıldı.
    2.  **Temel Değerlendirme Döngüsü:** Modern `llama_decode` (batch) yerine, `llama_eval` kullanılarak tek tek token işleme mantığına geri dönüldü. *(Not: Sonraki düzeltmelerle bunun `llama_decode` ve `llama_sampler_sample` olduğu teyit edildi.)*
    3.  **Doğru Fonksiyon İmzaları:** Tüm `llama_*` fonksiyon çağrıları, `b7046`'nın gerektirdiği doğru parametre türlerine (örn: `llama_tokenize` için `vocab*` yerine `model*`) göre düzeltildi.
    4.  **Eşzamanlılık Modelini Basitleştirme:** Sorunu izole etmek için `LlamaContextPool`, geçici olarak tek bir, mutex korumalı `llama_context` ile değiştirildi. Bu, karmaşık state yönetimi hatalarını ortadan kaldırdı.
    5.  **Öğrenilen Ders:** Harici bir C/C++ kütüphanesinin eski bir versiyonuna sabitlenirken, sadece `git checkout` yapmak yeterli değildir. O versiyona ait `include` dizinindeki başlık dosyaları, projenin tek ve mutlak referansı olarak kabul edilmelidir.

---
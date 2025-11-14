# 💡 Çözülmüş Sorunlar Veritabanı (Knowledge Base)

Bu doküman, geliştirme sırasında karşılaşılan önemli sorunları, kök nedenlerini ve çözümlerini "post-mortem" formatında belgeler.

---

### SORUN-001: API Uyumsuzluğu Nedeniyle Derleme Hataları (Eski Kayıt)

-   **Tarih:** 2025-11-11
-   **Belirtiler:** `llama_kv_cache_clear`, `llama_sample_token_greedy` gibi fonksiyonların bulunamaması.
-   **Kök Neden:** Proje kodu, `llama.cpp`'nin eski bir API setini kullanırken, `Dockerfile` en güncel sürümü çekiyordu.
-   **Çözüm:** `llm_engine.cpp` modern `llama.cpp` API'sine (`llama_sampler`, `llama_decode` vb.) göre güncellendi.

---

### SORUN-002: `libllama.so` / `libggml.so` Yüklenememe Hatası (Runtime)

-   **Tarih:** 2025-11-11
-   **Belirtiler:** Konteyner başlarken `error while loading shared libraries...` hatası.
-   **Kök Neden:** Multi-stage build'de `*.so` dosyaları `runtime` imajına kopyalanmıyordu.
-   **Çözüm:** `Dockerfile`'a `COPY --from=builder ... *.so` ve `RUN ldconfig` adımları eklendi.

---

### SORUN-003: `spdlog`/`fmt` Derleme Hatası (`Cannot format an argument`)

-   **Tarih:** 2025-11-11
-   **Belirtiler:** `static assertion failed: Cannot format an argument...`
-   **Kök Neden:** `spdlog` kütüphanesi, `grpc::StatusCode` enum'ını nasıl formatlayacağını bilmiyordu.
-   **Çözüm:** `grpc_client.cpp` dosyasına `fmt::formatter<grpc::StatusCode>` uzmanlaşması eklendi.

---

### SORUN-004 (REVİZE EDİLDİ): `llama.cpp` Bağımlılığındaki Stabilite Sorunları ve API Uyumsuzlukları

-   **Tarih:** 2025-11-14
-   **Belirtiler:** `llama_batch_add`, `llama_kv_cache_clear`, `llama_kv_cache_seq_rm` gibi fonksiyonlar için tekrarlanan `was not declared in this scope` hataları. `common` kütüphanesi için `cannot find -lcommon` ve `undefined reference` linkleme hataları.
-   **Kök Neden Analizi:**
    1.  **Kırılgan Bağımlılık:** Proje, `b7046` gibi otomatik oluşturulmuş ve API stabilitesi garantisi vermeyen bir `git tag`'ine sabitlenmişti. Bu versiyon, projenin orijinal olarak yazıldığı modern API'lerle uyumsuzdu.
    2.  **Geçici ve Hatalı Çözüm:** Bu uyumsuzluğu çözmek için, proje mimarisi basitleştirilmiş, `LlamaContextPool` devre dışı bırakılmış ve eski API'ye dönülmüştü. Bu, projenin temel hedeflerinden sapmasına neden olan bir **teknik borç** yarattı.
    3.  **CMake Anlaşmazlığı:** `llama.cpp`'nin `add_subdirectory` ile bir alt proje olarak kullanılması sırasında, `common` kütüphanesinin derlenmesi için `LLAMA_BUILD_COMMON=ON` bayrağının ayarlanması gerektiği tespit edilemedi. Bu durum, `common.cpp`'nin derlenmemesine ve linkleme hatalarına yol açtı.
-   **Nihai Çözüm:**
    1.  **Stabil Commit'e Geçiş:** `Dockerfile`'daki `LLAMA_CPP_VERSION`, `master` branch'ten alınmış, bilinen ve kararlı bir commit hash'ine (`92bb442...`) sabitlendi.
    2.  **Mimari Restorasyonu:** `LLMEngine`, modern `llama.cpp` API'sini kullanacak şekilde **sıfırdan yeniden yazıldı**. `LlamaContextPool` ve `ContextGuard` (RAII) yapıları, gerçek eşzamanlılığı sağlamak için yeniden implemente edildi.
    3.  **CMake Düzeltmesi:** Projenin `CMakeLists.txt` dosyası, `add_subdirectory(llama.cpp)` komutundan önce `set(LLAMA_BUILD_COMMON ON CACHE BOOL ... FORCE)` satırını ekleyerek `common` kütüphanesinin derlenmesini zorunlu hale getirdi. `target_link_libraries` komutuna `common` hedefi eklendi.
-   **Öğrenilen Ders:** Harici bir C/C++ projesini `add_subdirectory` ile kullanırken, o projenin CMake `option()` değişkenlerini (örn: `LLAMA_BUILD_COMMON`) ana projeden ayarlamak gerekebilir. Ayrıca, projenin API kontratını belgeleyen ve tek doğru kaynak olan bir `KB` dosyası (`04_LLAMA_CPP_API_BINDING.md`) oluşturmak, deneme-yanılmayı önler ve geliştirmeyi hızlandırır.

---

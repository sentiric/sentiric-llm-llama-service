# 💡 KB-02: Proje Desenleri ve Standartları

Bu doküman, projenin mimarisinde alınan temel kararları ve kodlama sırasında uyulması beklenen desenleri açıklar.

---

### 1. Eşzamanlılık Modeli: `LlamaContextPool`

-   **Amaç:** Birden çok isteği aynı anda ve birbirini engellemeden işlemek, böylece CPU kaynaklarından maksimum verim almak.
-   **Desen:** Bir "nesne havuzu" (object pool) desenidir. Servis başlangıcında maliyetli `llama_context` nesneleri oluşturulur ve bir havuza konur. Gelen her iş parçacığı (thread), havuzdan bir nesne "kiralar" (acquire), işini yapar ve işi bitince nesneyi havuza "iade eder" (release).
-   **Kritik Uygulama Detayı:** Bir context havuza iade edilmeden önce, içindeki KV cache'in temizlenmesi zorunludur. Aksi takdirde bir önceki isteğin state'i yeni isteğe sızar. Bu temizlik `llama_memory_seq_rm(llama_get_memory(ctx), -1, -1, -1);` komutu ile yapılır.

### 2. Build Stratejisi: "Clone-on-Build"

-   **Amaç:** Harici bağımlılıkları (özellikle sık güncellenen `llama.cpp`) yönetirken basitliği ve güncelliği sağlamak.
-   **Desen:** `Dockerfile`'ın `builder` aşamasında, `llama.cpp` reposu `git clone` ile doğrudan indirilir ve projenin geri kalanıyla birlikte derlenir.
-   **Gerekçe:** Bu yaklaşım, `git submodule` karmaşıklığından veya önceden derlenmiş Docker imajlarına bağımlı olmaktan kaçınır. Her build, kütüphanenin en son kararlı sürümünü kullanır. Dezavantajı, build sürelerinin biraz daha uzun olmasıdır, ancak bu CI/CD cache mekanizmalarıyla hafifletilebilir.

### 3. Bağımlılık Yönetimi: `vcpkg` + `CMake` + Docker

-   **`vcpkg` Sorumluluğu:** Platformlar arası C++ kütüphanelerini (`gRPC`, `spdlog`, `nlohmann-json` vb.) yönetir. `vcpkg.json` dosyası projenin bu kütüphanelere olan bağımlılığını tanımlar.
-   **`CMake` Sorumluluğu:** Projenin tamamının build sürecini yönetir. `vcpkg.cmake` toolchain dosyasını kullanarak `vcpkg` tarafından kurulan kütüphaneleri bulur. `add_subdirectory(llama.cpp)` ile `llama.cpp`'yi build'e dahil eder.
-   **Docker Sorumluluğu:** Tüm bu süreci izole ve tekrarlanabilir bir ortamda çalıştırır. Multi-stage build kullanarak son imajın boyutunu minimumda tutar.


---

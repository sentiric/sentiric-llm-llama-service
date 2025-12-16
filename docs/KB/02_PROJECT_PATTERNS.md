# 💡 KB-02: Proje Desenleri ve Standartları

Bu doküman, projenin mimarisinde alınan temel kararları ve kodlama sırasında uyulması beklenen desenleri açıklar.

---

### 1. Eşzamanlılık Modeli: `LlamaContextPool` (Akıllı Önbelleklemeli)

-   **Amaç:** Birden çok isteği aynı anda ve birbirini engellemeden işlemek, GPU/CPU kaynaklarından maksimum verim almak ve ardışık isteklerde performansı artırmak.
-   **Desen:** Gelişmiş bir "nesne havuzu" (object pool) desenidir. Gelen her istek, havuza kendi prompt token'larını sunar. Havuz, boşta olan context'ler arasında, gelen isteğin başlangıcıyla en çok eşleşen token dizisine sahip olanı bulur ve onu kiralar. Bu "Akıllı Önbellek" (Smart Context Caching) mekanizması, prompt'un eşleşen kısmının yeniden işlenmesini (re-decode) önleyerek TTFT'yi (Time To First Token) ciddi şekilde düşürür.
-   **Kritik Uygulama Detayı:** Bir context kiralanırken, yeni isteğin token'ları ile eski önbelleklenmiş token'ları karşılaştırılır. Eğer `M` token eşleşirse, `llama_memory_seq_rm` fonksiyonu çağrılarak KV önbelleğinin `M` token'dan sonrası temizlenir. Bu, bir önceki isteğin state'inin yeni isteğe sızmasını engeller. İşlem bittiğinde, context'in son token durumu bir sonraki istek için havuza kaydedilir.

### 2. Build Stratejisi: "Clone-on-Build"

-   **Amaç:** Harici bağımlılıkları (özellikle sık güncellenen `llama.cpp`) yönetirken basitliği ve güncelliği sağlamak.
-   **Desen:** `Dockerfile`'ın `builder` aşamasında, `llama.cpp` reposu `git clone` ile doğrudan indirilir ve projenin geri kalanıyla birlikte derlenir.
-   **Gerekçe:** Bu yaklaşım, `git submodule` karmaşıklığından veya önceden derlenmiş Docker imajlarına bağımlı olmaktan kaçınır. Her build, kütüphanenin sabitlenmiş kararlı sürümünü kullanır. Dezavantajı, build sürelerinin biraz daha uzun olmasıdır, ancak bu CI/CD cache mekanizmalarıyla hafifletilebilir.

### 3. Bağımlılık Yönetimi: `vcpkg` + `CMake` + Docker

-   **`vcpkg` Sorumluluğu:** Platformlar arası C++ kütüphanelerini (`gRPC`, `spdlog`, `nlohmann-json` vb.) yönetir. `vcpkg.json` dosyası projenin bu kütüphanelere olan bağımlılığını tanımlar.
-   **`CMake` Sorumluluğu:** Projenin tamamının build sürecini yönetir. `vcpkg.cmake` toolchain dosyasını kullanarak `vcpkg` tarafından kurulan kütüphaneleri bulur. `add_subdirectory(llama.cpp)` ile `llama.cpp`'yi build'e dahil eder.
-   **Docker Sorumluluğu:** Tüm bu süreci izole ve tekrarlanabilir bir ortamda çalıştırır. Multi-stage build kullanarak son imajın boyutunu minimumda tutar.

---
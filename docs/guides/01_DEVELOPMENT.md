# 👨‍💻 Geliştirici Rehberi

## Build Sistemi

### Bağımlılık Yönetimi
-   **vcpkg**: `gRPC`, `spdlog`, `httplib` gibi temel C++ bağımlılıklarını yönetir.
-   **`llama.cpp`**: Docker build sırasında sabitlenmiş bir commit'ten klonlanır ve anlık olarak derlenir. Proje, `llama.cpp`'yi bir alt dizin olarak kullanır.
-   **Protobuf**: `CMake`'in `FetchContent` modülü, `sentiric-contracts` reposunu derleme anında çeker ve `protoc` ile gRPC/Protobuf kodlarını üretir.

### CMake Yapılandırması
`CMakeLists.txt` dosyası, `vcpkg` toolchain'i kullanarak bağımlılıkları bulur. `llama.cpp` projesini `add_subdirectory` ile dahil eder ve `LLAMA_BUILD_COMMON=ON` bayrağını ayarlayarak `common` kütüphanesinin derlenmesini sağlar. `llm_service` ve `llm_cli` hedefleri, hem `llama` hem de `common` kütüphanelerine linklenir.

### Docker Best Practices
-   **Multi-stage Build:** `builder` aşaması tüm derleme araçlarını içerir. `runtime` aşaması ise sadece çalıştırılabilir dosyaları ve gerekli paylaşılan kütüphaneleri (`*.so`) içerir.
-   **Dinamik Kütüphane Yönetimi:** `runtime` imajı, `ldconfig` komutunu çalıştırarak dinamik linker önbelleğini günceller.

---

## Kod Standartları ve API Kullanımı

-   **C++17** standardı zorunludur.
-   **RAII** prensibi benimsenmelidir.
-   **`llama.cpp` API Kullanımı:** Projenin kullandığı `llama.cpp` versiyonu için tüm temel API kullanım desenleri **`docs/KB/04_LLAMA_CPP_API_BINDING.md`** dosyasında belgelenmiştir. Bu dosya, `llama.cpp` ile etkileşimde tek doğru kaynaktır.

---

## Geliştirme Sırasında Sık Kullanılan Komutlar

### 1. Servisi Derleme ve Başlatma

Her kod değişikliğinden sonra bu komut çalıştırılmalıdır.

```bash
# CPU için
docker compose up --build -d

# GPU için
docker compose -f docker-compose.yml -f docker-compose.gpu.yml -f docker-compose.gpu.override.yml up --build -d
```

### 2. `llm_cli` Aracını Çalıştırma

`llm_cli`'yi çalıştırmak için `docker compose run` komutu kullanılır. Bu komut, ilgili ortam için (`cpu` veya `gpu`) tek seferlik bir konteyner başlatır.

#### CPU Ortamında CLI Kullanımı

CPU için ek bir dosyaya gerek yoktur. `docker-compose.override.yml` dosyası, `llm_cli` için gerekli tanımı içerir (bir sonraki adımda ekleyeceğiz).

```bash
# CPU'da CLI çalıştırma
docker compose run --rm llm-cli llm_cli <komut>
```

#### GPU Ortamında CLI Kullanımı

GPU ortamında `llm-cli`'yi çalıştırmak için, `docker-compose.run.gpu.yml` dosyasını özel olarak belirtmeniz gerekir. Bu dosya, konteynere GPU erişimi sağlar ve çalışan servisin ağına bağlanır.

```bash
# GPU'da CLI çalıştırma
docker compose -f docker-compose.run.gpu.yml run --rm llm-cli llm_cli <komut>
```

**Örnek `generate` komutu (GPU):**
```bash
docker compose -f docker-compose.run.gpu.yml run --rm llm-cli llm_cli generate "Sen, Sentiric platformunda çalışan, yardımsever ve profesyonel bir AI asistansın. Cevapların her zaman kısa (en fazla 2 cümle), net ve samimi olsun." --timeout 120
```

```bash
docker compose -f docker-compose.run.gpu.yml run --rm llm-cli llm_cli generate "Kullanıcıyı ismiyle ({user_name}) sıcak bir şekilde karşıla ve nasıl yardımcı olabileceğini tek bir kısa cümlede sor." --timeout 120
```

```bash
docker compose -f docker-compose.run.gpu.yml run --rm llm-cli llm_cli generate "Yeni bir kullanıcıyı sıcak bir şekilde karşıla ve nasıl yardımcı olabileceğini tek bir kısa cümlede sor." --timeout 120
```

```bash
docker compose -f docker-compose.run.gpu.yml run --rm llm-cli llm_cli generate "Yeni bir kullanıcıyı sıcak bir şekilde karşıla ve nasıl yardımcı olabileceğini tek bir kısa cümlede sor."
```

```bash
docker compose -f docker-compose.run.gpu.yml run --rm llm-cli llm_cli generate "Sana sağlanan "### İlgili Bilgiler" metnindeki bilgileri kullanarak kullanıcının sorusuna cevap ver. Eğer aradığı bilgi metinde yoksa, bunu kibarca "Bu konuda size yardımcı olacak detaylı bilgiye sahip değilim, farklı bir konuda yardımcı olabilir miyim?" gibi bir ifadeyle belirt. Cevapların her zaman kısa (en fazla 2 cümle), net ve samimi olsun. Asla tahmin yürütme.\n\n### İlgili Bilgiler:\n{context}\n\n### Kullanıcının Sorusu:\n{query}\n\n### Cevap:" --timeout 120
```

```bash
docker compose -f docker-compose.run.gpu.yml run --rm llm-cli llm_cli generate "Sana sağlanan "### İlgili Bilgiler" metnindeki bilgileri kullanarak kullanıcının sorusuna cevap ver. Eğer aradığı bilgi metinde yoksa, bunu kibarca belirt. Cevapların her zaman kısa (en fazla 2 cümle), net ve samimi olsun. Asla tahmin yürütme.\n\n### İlgili Bilgiler:\n{context}\n\n### Kullanıcının Sorusu:\n{query}\n\n### Cevap:" --timeout 120
```

Kayıtlı Türkçe templates tablosu
---
('PROMPT_SYSTEM_DEFAULT', 'system', 'tr', 'system_prompt', 'Sen, Sentiric platformunda çalışan, yardımsever ve profesyonel bir AI asistansın. Cevapların her zaman kısa (en fazla 2 cümle), net ve samimi olsun. Asla bir yapay zeka olduğunu belirtme. Aşağıdaki konuşma geçmişine göre diyaloğa devam et.', 'Varsayılan Türkçe sistem promptu (RAG bilgisi olmadığında kullanılır)'),
('PROMPT_WELCOME_KNOWN_USER', 'system', 'tr', 'prompt', 'Kullanıcıyı ismiyle ({user_name}) sıcak bir şekilde karşıla ve nasıl yardımcı olabileceğini tek bir kısa cümlede sor.', 'Tanınan kullanıcı için doğal ve kısa Türkçe karşılama promptu'),
('PROMPT_WELCOME_GUEST', 'system', 'tr', 'prompt', 'Yeni bir kullanıcıyı sıcak bir şekilde karşıla ve nasıl yardımcı olabileceğini tek bir kısa cümlede sor.', 'Misafir kullanıcı için doğal ve kısa Türkçe karşılama promptu'),
('PROMPT_SYSTEM_RAG', 'system', 'tr', 'system_prompt', 'Sana sağlanan "### İlgili Bilgiler" metnindeki bilgileri kullanarak kullanıcının sorusuna cevap ver. Eğer aradığı bilgi metinde yoksa, bunu kibarca "Bu konuda size yardımcı olacak detaylı bilgiye sahip değilim, farklı bir konuda yardımcı olabilir miyim?" gibi bir ifadeyle belirt. Asla tahmin yürütme.\n\n### İlgili Bilgiler:\n{context}\n\n### Kullanıcının Sorusu:\n{query}\n\n### Cevap:', 'RAG için hem doğal hem de güvenli Türkçe sistem promptu'),

---

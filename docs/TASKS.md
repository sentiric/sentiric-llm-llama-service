# 📋 LLM Llama Service - Anayasa Uyum ve Geliştirme Görev Listesi (Revizyon 2)

**Belge Amacı:** Bu doküman, `sentiric-llm-llama-service` projesinin, `sentiric-governance` anayasasında belirtilen mimari, güvenlik ve operasyonel standartlara tam uyumlu, "production-ready" bir bileşen haline getirilmesi için gereken tüm teknik görevleri tanımlar. Görevler, risk ve etki seviyelerine göre önceliklendirilmiştir. Her görev, bu repoya odaklanmış bir geliştiricinin ihtiyaç duyacağı tüm bağlamı içerecek şekilde tasarlanmıştır.

**Önceliklendirme Kılavuzu:**
-   **🟥 KRİTİK (CRITICAL):** Servisin temel işlevselliğini, güvenliğini veya anayasa uyumunu doğrudan etkileyen, derhal müdahale gerektiren görevler. Bu görevler tamamlanmadan başka bir işe başlanmamalıdır.
-   **🟧 YÜKSEK (HIGH):** Platformun kararlılığını, güvenilirliğini veya uzun vadeli bakımını ciddi şekilde etkileyen görevler. Kritik görevler biter bitmez ele alınmalıdır.
-   **🟨 ORTA (MEDIUM):** Mimari temizliği, test edilebilirliği ve gelecekteki genişletilebilirliği artıran önemli refactoring görevleri.
-   **🟦 DÜŞÜK (LOW):** Gelecek entegrasyonları ve operasyonel kolaylıkları hedefleyen iyileştirmeler.

---

## 🟥 KRİTİK GÖREVLER (CRITICAL)

### **TASK ID: `LLM-SEC-001`**
-   **BAŞLIK:** Güvenlik Zafiyetini Gider: gRPC İletişimini mTLS ile Şifrele
-   **ETİKETLER:** `security`, `bug`, `governance-violation`
-   **GEREKÇE:** Platform anayasası (`governance` reposu, `Service-Communication-Architecture.md`), tüm iç gRPC iletişiminin mTLS ile şifrelenmesini **zorunlu** kılar. Mevcut `InsecureServerCredentials` kullanımı, bu kuralı ihlal eden ve servisi ağ içi saldırılara karşı savunmasız bırakan kritik bir güvenlik açığıdır.
-   **ÖNERİLEN ÇÖZÜM:** `grpc_server.cpp` ve `cli/grpc_client.cpp` dosyaları, platformun merkezi `sentiric-certificates` reposu tarafından sağlanan sertifikaları kullanarak güvenli (`SslServerCredentials` ve `SslChannelCredentials`) kanallar oluşturmalıdır. Sertifika yolları, `sentiric-config` tarafından tanımlanacak ortam değişkenleri (`LLM_LLAMA_SERVICE_CERT_PATH`, `LLM_LLAMA_SERVICE_KEY_PATH`, `GRPC_TLS_CA_PATH`) üzerinden okunmalıdır.
-   **KABUL KRİTERLERİ:**
    -   [ ] `grpc::Insecure...Credentials()` çağrıları kod tabanından tamamen kaldırılmıştır.
    -   [ ] Sunucu, istemci sertifikası doğrulaması (`GRPC_SSL_REQUEST_AND_REQUIRE_CLIENT_CERTIFICATE_AND_VERIFY`) talep eden `SslServerCredentials` ile başlatılmıştır.
    -   [ ] `llm_cli` istemcisi, geçerli istemci sertifikaları olmadan sunucuya bağlanamamalıdır.
    -   [ ] Geçerli sertifikalarla şifreli iletişim başarıyla kurulmalıdır.
-   **DIŞ BAĞIMLILIKLAR:**
    -   **`sentiric-infrastructure`:** `docker-compose.*.yml` dosyası, `sentiric-certificates` reposunu `/sentiric-certificates` olarak konteynere mount etmelidir.
    -   **`sentiric-config`:** `services/llm-llama-service.env` dosyası, ilgili sertifika yollarını içeren değişkenleri tanımlamalıdır.

### **TASK ID: `LLM-BUG-001`**
-   **BAŞLIK:** Kritik API Hatasını Düzelt: gRPC İstek Parametreleri Yok Sayılıyor
-   **ETİKETLER:** `bug`, `api-contract`, `critical-functionality`
-   **GEREKÇE:** Servis, istemciden gelen `temperature`, `top_k`, `top_p` gibi temel sampling parametrelerini yok sayarak API kontratını ihlal etmektedir. Bu, servisin temel işlevini yerine getirmesini engeller.
-   **ÖNERİLEN ÇÖZÜM:** `llm_engine.cpp` içerisindeki `generate_stream` metodu, `sentiric-contracts`'da tanımlı `GenerationParams` mesajını işlemeli, isteğe özel bir `llama_sampler` zinciri oluşturmalı ve token üretimini bu dinamik örnekleyici ile yapmalıdır.
-   **KABUL KRİTERLERİ:**
    -   [ ] `temperature=0.0` ve `top_k=1` ile gönderilen bir istek, aynı prompt için her zaman deterministik (aynı) bir çıktı üretmelidir.
    -   [ ] `max_new_tokens` parametresi doğru bir şekilde uygulanmalı ve üretilen token sayısı bu limiti aşmamalıdır.

---

## 🟧 YÜKSEK ÖNCELİKLİ GÖREVLER (HIGH)

### **TASK ID: `LLM-OPS-001`**
-   **BAŞLIK:** Gözlemlenebilirlik Standardını Uygula: Ortama Duyarlı Yapılandırılmış Loglama
-   **ETİKETLER:** `observability`, `logging`, `governance-violation`
-   **GEREKÇE:** Platform anayasası (`governance` reposu, `OBSERVABILITY_STANDARD.md`), üretim ortamı için **JSON** formatında loglamayı zorunlu kılar. Bu, üretimde sorun giderme ve otomatik izleme için esastır.
-   **ÖNERİLEN ÇÖZÜM:** `main.cpp` dosyasına, `sentiric-config`'den gelecek olan `ENV` ortam değişkenini okuyan bir mantık eklenmelidir. `ENV=production` ise `spdlog` JSON formatında, `ENV=development` ise insan tarafından okunabilir renkli formatta log basacak şekilde yapılandırılmalıdır.
-   **KABUL KRİTERLERİ:**
    -   [ ] `ENV=production` ayarıyla çalıştırıldığında, servis logları `stdout`'a geçerli JSON objeleri olarak yazılmalıdır.
    -   [ ] `ENV=development` ayarıyla çalıştırıldığında, loglar konsolda okunabilir formatta görünmelidir.
    -   [ ] Tüm log kayıtları, standartta belirtilen `timestamp`, `level`, `service`, `message`, `trace_id` (varsa) alanlarını içermelidir.

### **TASK ID: `LLM-BUILD-001`**
-   **BAŞLIK:** Build Kırılganlığını Gider: `llama.cpp` Bağımlılığını Sabitle
-   **ETİKETLER:** `build`, `ci-cd`, `stability`
-   **GEREKÇE:** `Dockerfile`'ın `llama.cpp`'yi doğrudan `master` branch'ten klonlaması, tekrarlanabilir build'leri engeller. Harici repodaki bir API değişikliği, projenin derlenmesini aniden bozabilir.
-   **ÖNERİLEN ÇÖZÜM:** `Dockerfile` içerisindeki `git clone` komutundan sonra, `git checkout <commit_hash_veya_tag>` komutu eklenerek `llama.cpp`'nin bilinen ve test edilmiş belirli bir versiyonu kullanılmalıdır.
-   **KABUL KRİTERLERİ:**
    -   [ ] `Dockerfile` içinde `llama.cpp` için belirli bir commit hash'i veya git tag'i kullanılmaktadır.
    -   [ ] Build süreci, `llama.cpp`'nin `master` branch'indeki anlık değişikliklerden etkilenmemelidir.

### **TASK ID: `LLM-TEST-001`**
-   **BAŞLIK:** Güvenilir Olmayan `benchmark` Aracını Yeniden Yaz
-   **ETİKETLER:** `testing`, `performance`, `bug`
-   **GEREKÇE:** Mevcut benchmark aracı, `tokens_per_second` metriğini yanlış hesaplayarak sistemin performansı hakkında yanıltıcı sonuçlar üretmektedir.
-   **ÖNERİLEN ÇÖZÜM:** `run_performance_test` metodu, `on_token` callback'ini kullanarak üretilen gerçek token sayısını saymalı ve `tokens_per_second` metriğini bu veriye göre (`toplam_token / toplam_süre`) hesaplamalıdır.
-   **KABUL KRİTERLERİ:**
    -   [ ] `tokens_per_second` hesaplamasında keyfi sabitler kullanılmamaktadır.
    -   [ ] Benchmark raporu, üretilen toplam token sayısını ve gerçek saniye başına token hızını göstermektedir.

---

## 🟨 ORTA ÖNCELİKLİ GÖREVLER (MEDIUM)

### **TASK ID: `LLM-REFACTOR-001`**
-   **BAŞLIK:** Mimariyi İyileştir: `ModelManager` Sorumluluğunu `LLMEngine` İçine Taşı
-   **ETİKETLER:** `refactor`, `architecture`, `governance-compliance`
-   **GEREKÇE:** `main.cpp`'nin, `LLMEngine`'in temel bağımlılığı olan modelin hazırlanması görevini üstlenmesi, `governance`'da belirtilen "Tek Sorumluluk Prensibi"ni ihlal eder. Bu durum test edilebilirliği azaltır.
-   **ÖNERİLEN ÇÖZÜM:** `ModelManager::ensure_model_is_ready` çağrısı `main.cpp`'den kaldırılarak `LLMEngine`'in constructor'ı içine taşınmalıdır. `LLMEngine`, kendi bağımlılıklarını kendisi yönetmelidir.
-   **KABUL KRİTERLERİ:**
    -   [ ] `main.cpp` dosyasında `ModelManager`'a hiçbir referans kalmamıştır.
    -   [ ] `LLMEngine`'in constructor'ı, modelin indirilmesi veya doğrulanması sürecini yönetmektedir.
    -   [ ] Servis, bu değişiklik sonrasında fonksiyonel olarak doğru çalışmaya devam etmektedir.

### **TASK ID: `LLM-API-001`**
-   **BAŞLIK:** API Kontratını Zenginleştir: `llm-gateway` Entegrasyonuna Hazırla
-   **ETİKETLER:** `api-contract`, `feature`, `architecture`, `external-dependency`
-   **GEREKÇE:** Platform mimarisi, bu servisin `llm-gateway-service` tarafından kullanılmasını öngörür. `llm-gateway`'in maliyet/performans optimizasyonu yapabilmesi için model seçimi gibi ek parametrelere ihtiyacı olacaktır.
-   **ÖNERİLEN ÇÖZÜM:** Bu görev bu reponun sorumluluğunda değildir ancak bilinmesi kritiktir. `sentiric-contracts` reposunda, `LocalGenerateStreamRequest` mesajı `model_selector` gibi yeni alanlar içerecek şekilde güncellenmelidir. Bu servis, o değişiklik yapıldığında yeni kontrata uyum sağlamalıdır.
-   **KABUL KRİTERLERİ:**
    -   [ ] **(Bu Repoda)** `llm-llama-service`, `sentiric-contracts` v1.11.0 (veya üstü) sürümüne güncellenmiştir.
    -   [ ] **(Bu Repoda)** `llm_engine`, istekle gelen `model_selector` alanını loglayabilir (gelecekteki işlevsellik için hazırlık).
-   **DIŞ BAĞIMLILIKLAR:**
    -   **`sentiric-contracts`:** `local.proto` dosyasının güncellenmesi ve yeni bir sürüm yayınlanması gerekmektedir.

---

## 🟦 DÜŞÜK ÖNCELİKLİ GÖREVLER (LOW)

### **TASK ID: `LLM-FEATURE-001`**
-   **BAŞLIK:** VCA Entegrasyonu: Token Kullanım İstatistiklerini Döndür
-   **ETİKETLER:** `feature`, `vca`, `governance-compliance`
-   **GEREKÇE:** Platformun Değer ve Maliyet Analizi (VCA) motoru (`governance` ADR-006), her işlemin maliyetini hesaplamak zorundadır. Bu servisin, ne kadar kaynak (token) tükettiğini bildirmesi gerekir.
-   **ÖNERİLEN ÇÖZÜM:** `llm_engine.cpp`, bir stream tamamlandığında üretilen prompt ve tamamlama token'larının sayısını hesaplamalıdır. Bu bilgi, `sentiric-contracts`'daki `LocalGenerateStreamResponse`'un `FinishDetails` mesajına eklenmelidir.
-   **KABUL KRİTERLERİ:**
    -   [ ] Stream sonlandığında gönderilen `FinishDetails` mesajı, `prompt_tokens` ve `completion_tokens` gibi alanları içermektedir.
-   **DIŞ BAĞIMLILIKLAR:**
    -   **`sentiric-contracts`:** `local.proto` dosyasındaki `FinishDetails` mesajının güncellenmesi gerekmektedir.
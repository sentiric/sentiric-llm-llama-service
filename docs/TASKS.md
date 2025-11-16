# 📋 LLM Llama Service - Anayasa Uyum ve Geliştirme Görev Listesi (Revizyon 3)

**Belge Amacı:** Bu doküman, `sentiric-llm-llama-service` projesinin, `sentiric-governance` anayasasında belirtilen mimari standartlara tam uyumlu, "production-ready" bir bileşen haline getirilmesi için gereken görevleri tanımlar.

---

## ✅ TAMAMLANAN GÖREVLER

-   **[✓] TASK ID: `LLM-SEC-001`**: gRPC iletişimi mTLS ile şifrelendi.
-   **[✓] TASK ID: `LLM-BUG-001`**: gRPC istek parametreleri (sampling) artık dinamik olarak işleniyor.
-   **[✓] TASK ID: `LLM-OPS-001`**: Ortama duyarlı yapılandırılmış loglama (JSON/konsol) implemente edildi.
-   **[✓] TASK ID: `LLM-BUILD-001`**: `llama.cpp` bağımlılığı belirli bir commite sabitlendi.
-   **[✓] TASK ID: `LLM-REFACTOR-001`**: `ModelManager` sorumluluğu `LLMEngine` içine taşındı, mimari temizlendi.

---

## 🟥 AKTİF GÖREV (CRITICAL)

### **TASK ID: `LLM-API-002`**
-   **BAŞLIK:** API Kontratını Nihai Mimariye Yükselt: Zengin Diyalog Yönetimini Destekle
-   **ETİKETLER:** `refactor`, `api-contract`, `architecture`, `governance-compliance`
-   **GEREKÇE:** Mevcut API kontratı, sadece tek bir `prompt` string'i alarak platformun "Tak-Çıkar Lego Seti" ve "Tek Sorumluluk" prensiplerini ihlal etmektedir. Platformun `agent-service` ve `llm-gateway-service` gibi üst katman servislerinin, karmaşık diyalogları (konuşma geçmişi, RAG bağlamı vb.) yönetebilmesi için bu servisin, yapılandırılmış ve zengin bir istek modelini desteklemesi zorunludur.
-   **ÖNERİLEN ÇÖZÜM:**
    1.  Projenin `CMakeLists.txt` dosyası, `sentiric-contracts` reposunun en güncel versiyonunu (`v1.11.0+`) kullanacak şekilde güncellenmelidir.
    2.  `PromptFormatter` sınıfı, yeni `LLMLocalServiceGenerateStreamRequest` mesajını alıp, `system_prompt`, `user_prompt`, `rag_context` ve `history` alanlarını birleştirerek modele özgü nihai prompt metnini oluşturacak şekilde yeniden yazılmalıdır.
    3.  `LLMEngine` ve `GrpcServer` sınıfları, bu yeni, zengin istek ve yanıt mesajlarıyla çalışacak şekilde güncellenmelidir.
    4.  `llm_cli` aracı, yeni mimariyi test edebilmek için `--system-prompt`, `--user-prompt` gibi yeni argümanları destekleyecek şekilde güncellenmelidir.
-   **KABUL KRİTERLERİ:**
    -   [ ] Proje, `sentiric-contracts v1.11.0` ile başarıyla derlenmektedir.
    -   [ ] `PromptFormatter`, RAG ve konuşma geçmişi senaryolarını doğru bir şekilde formatlamaktadır.
    -   [ ] `llm_cli` aracı ile hem basit hem de RAG destekli istekler başarıyla gönderilebilmektedir.
    -   [ ] Servis, telefon diyaloglarını destekleyecek esnekliğe ve doğruluğa kavuşmuştur.
-   **DIŞ BAĞIMLILIKLAR:**
    -   **`sentiric-contracts` (v1.11.0):** `local.proto` ve `gateway.proto` dosyalarının nihai mimariye uygun olması.
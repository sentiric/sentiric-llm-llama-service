# 📋 LLM Llama Service - Anayasa Uyum ve Geliştirme Görev Listesi (Revizyon 4 - Tamamlandı)

**Belge Amacı:** Bu doküman, `sentiric-llm-llama-service` projesinin, `sentiric-governance` anayasasında belirtilen mimari standartlara tam uyumlu, "production-ready" bir bileşen haline getirilmesi için gereken görevleri tanımlar.

---

## ✅ TAMAMLANAN GÖREVLER

-   **[✓] TASK ID: `LLM-SEC-001`**: gRPC iletişimi mTLS ile şifrelendi.
-   **[✓] TASK ID: `LLM-BUG-001`**: gRPC istek parametreleri (sampling) artık dinamik olarak işleniyor.
-   **[✓] TASK ID: `LLM-OPS-001`**: Ortama duyarlı yapılandırılmış loglama (JSON/konsol) implemente edildi.
-   **[✓] TASK ID: `LLM-BUILD-001`**: `llama.cpp` bağımlılığı belirli bir commite sabitlendi.
-   **[✓] TASK ID: `LLM-REFACTOR-001`**: `ModelManager` sorumluluğu `LLMEngine` içine taşındı, mimari temizlendi.
-   **[✓] TASK ID: `LLM-API-002`**: API kontratı, `sentiric-contracts v1.11.0` ile uyumlu hale getirilerek zengin diyalog yönetimi yeteneği kazandırıldı.
-   **[✓] TASK ID: `LLM-FEATURE-001`**: VCA entegrasyonu tamamlandı. Servis artık stream sonunda `prompt_tokens` ve `completion_tokens` sayılarını raporlamaktadır.

---

**Tüm planlanan görevler tamamlanmıştır. Servis, üretim ortamına dağıtılmaya hazırdır.**

---

#### **FAZ 1: TEMELİ GÜÇLENDİRME VE KÖPRÜYÜ İNŞA ETME**

-   **[ ] TASK ID: `LLM-API-003` - OpenAI Uyumlu API Endpoint'i Oluşturma**
    *   **Açıklama:** `http_server.cpp`'ye, OpenAI API standardıyla %100 uyumlu bir `/v1/chat/completions` endpoint'i eklenecektir. Bu endpoint hem streaming hem de non-streaming cevapları destekleyecektir.
    *   **Kabul Kriterleri:**
        *   `llm-llama-service`, Postman veya benzeri bir araçla gönderilen standart bir OpenAI `chat/completions` isteğine başarılı bir şekilde cevap verebilmelidir.
        *   İstek formatı (`{"messages": [...]}`) ve cevap formatı (`{"choices": [...]}`) OpenAI dokümantasyonu ile birebir uyumlu olmalıdır.
        *   **Güvenlik Notu:** Bu endpoint doğrudan dış dünyaya açılmayacak, platformun gelecekteki `api-gateway-service`'i tarafından kullanılmak üzere tasarlanacaktır.

-   **[ ] TASK ID: `LLM-PERF-001` - Adaptif Dinamik Batching Mekanizmasını Uygulama**
    *   **Açıklama:** `llm_engine`, gelen istekleri donanım kapasitesine göre akıllıca gruplayarak GPU'ya tek seferde gönderecek "Adaptif Batching" mekanizmasını implemente edecektir.
    *   **Kabul Kriterleri:**
        *   Servis başlangıcında, `n_gpu_layers` ve mevcut VRAM'e göre mantıklı bir `max_batch_size` değeri otomatik olarak belirlenmelidir.
        *   Düşük VRAM'li sistemlerde (örn: 6GB GPU), `max_batch_size` otomatik olarak `1` veya `2` gibi güvenli bir değere ayarlanmalıdır.
        *   Yüksek VRAM'li sistemlerde, `docker-compose.yml`'de belirtilen `LLM_LLAMA_SERVICE_MAX_BATCH_SIZE` değerini kullanmalıdır.
        *   `llm_cli benchmark --concurrent 4` gibi bir komutla, batching'in saniye başına token (TPS) üretimini artırdığı kanıtlanmalıdır.

---

#### **FAZ 2: VİZYONU PROTOTİPLEME**

-   **[ ] TASK ID: `UI-PRO-001` - "Sentiric Studio" Prototip Arayüzünü Geliştirme**
    *   **Açıklama:** Deepseek AI tarafından tasarlanan HTML/CSS temel alınarak, Vue.js veya React kullanılarak fonksiyonel bir web arayüzü oluşturulacaktır. Bu arayüz, Faz 1'de oluşturulan `/v1/chat/completions` endpoint'ini kullanarak `llm-llama-service` ile konuşacaktır.
    *   **Kabul Kriterleri (İlk Sürüm için):**
        *   Kullanıcı, ana sohbet ekranı üzerinden mesaj gönderip anlık (streaming) cevap alabilmelidir.
        *   Sol paneldeki "Sistem Promptu" ve "RAG Context" alanları çalışır durumda olmalı ve `llm-llama-service`'e gönderilen isteğe dahil edilmelidir.
        *   Sağ paneldeki metrikler (TTFT, TPS) yaklaşık olarak hesaplanıp gösterilmelidir.
        *   Bu arayüz, gelecekte `governance`'da tanımlanan diğer servislerle (örn: `knowledge-service`'ten RAG kaynaklarını listelemek) konuşabilecek şekilde modüler tasarlanmalıdır.

---


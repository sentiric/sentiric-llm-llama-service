# 📋 Sentiric LLM Llama Service - Görev ve Yol Haritası

**Belge Amacı:** Bu doküman, `sentiric-llm-llama-service` projesinin tamamlanan, üzerinde çalışılan ve planlanan tüm görevlerini takip etmek için kullanılan tek doğruluk kaynağıdır. Projenin genel vizyonu, `sentiric-governance` reposunda tanımlanan "İletişim İşletim Sistemi" mimarisine hizmet etmektir.

---

## ✅ TAMAMLANAN GÖREVLER (FAZ 1 - Temeli İnşa Etme)

Bu faz, servisi sıfırdan üretime hazır, kararlı ve yüksek performanslı bir temel üzerine oturtmuştur.

-   **[✓] TASK ID: `LLM-BUILD-001` - Tekrarlanabilir Build Altyapısı Kurulumu**
    *   **Açıklama:** Proje, `vcpkg` bağımlılık yönetimi, `CMake` derleme sistemi ve çok aşamalı `Dockerfile`'lar (CPU/GPU) ile sağlam ve tekrarlanabilir bir build sürecine kavuşturuldu.
    *   **Sonuç:** Her ortamda tutarlı derlemeler garanti altına alındı.

-   **[✓] TASK ID: `LLM-CORE-001` - Eşzamanlı İstek Mimarisi (Context Pool)**
    *   **Açıklama:** `LlamaContextPool` ve RAII tabanlı `ContextGuard` yapıları implemente edilerek, servisin birden çok isteği aynı anda ve birbirini engellemeden işleyebilmesi sağlandı.
    *   **Sonuç:** Servis, çok kullanıcılı senaryolar için ölçeklenebilir bir temele oturtuldu.

-   **[✓] TASK ID: `LLM-STBL-001` - Kritik Bellek Hatalarının Giderilmesi (SegFault)**
    *   **Açıklama:** Servisin yük altında `exit code 139` (Segmentation Fault) ile çökmesine neden olan kritik bellek yönetimi hataları, RAII prensipleri ve güvenli kaynak temizleme mantığı ile tamamen çözüldü.
    *   **Sonuç:** Servis, art arda gelen istekler altında bile %100 kararlı hale getirildi.

-   **[✓] TASK ID: `LLM-API-003` - OpenAI Uyumlu API Endpoint'i Entegrasyonu**
    *   **Açıklama:** `HttpServer`'a, endüstri standardı olan `/v1/chat/completions` endpoint'i eklendi. Bu endpoint, hem streaming hem de non-streaming modlarını desteklemektedir.
    *   **Sonuç:** Proje, binlerce harici araç ve UI ile doğrudan entegre olabilecek evrensel bir arayüze kavuştu.

-   **[✓] TASK ID: `LLM-PERF-001` - Kararlı Dinamik Batching Mekanizması**
    *   **Açıklama:** `DynamicBatcher` mimarisi, gelen istekleri gruplayarak motorun daha verimli çalışmasını sağlayacak şekilde entegre edildi. Bellek hatalarını önlemek için karmaşık paralel decode yerine kararlı bir ardışık işleme modeli benimsendi.
    *   **Sonuç:** Servisin verimliliği (throughput), kararlılıktan ödün verilmeden artırıldı.

-   **[✓] TASK ID: `UI-PRO-001` - "Sentiric Studio" MVP Arayüzünün Geliştirilmesi**
    *   **Açıklama:** Eski `web/` arayüzü tamamen kaldırılarak yerine Vue.js 3 tabanlı, modern, profesyonel ve fonksiyonel bir "Stüdyo" arayüzü (`studio/`) geliştirildi.
    *   **Sonuç:** Projenin yeteneklerini sergileyen ve ileri düzey testler için bir laboratuvar görevi gören bir vitrin oluşturuldu.

---

## ⏳ ÜZERİNDE ÇALIŞILAN GÖREVLER

-   **[ ] TASK ID: `LLM-VAL-001` - Sentiric Studio v1.0 MVP'nin Kapsamlı Test ve Doğrulaması**
    *   **Açıklama:** Yeni `studio/` arayüzü üzerinden, servisin tüm temel yeteneklerinin beklendiği gibi çalıştığını doğrulamak.
    *   **Kabul Kriterleri:**
        *   **[ ] Temel Sohbet Testi:** Arka planın basit sorulara akıcı ve doğru cevaplar verdiği doğrulanmalı.
        *   **[ ] Sistem Promptu Testi:** Sistemin davranışının, verilen prompt'a göre değiştiği gözlemlenmeli.
        *   **[ ] RAG Context Testi:** Modelin, verilen harici bilgiye sadık kalarak cevap ürettiği kanıtlanmalı.
        *   **[ ] Parametre Testi:** `Sıcaklık` ve `Maksimum Token` gibi ayarların, üretilen cevabın yapısını değiştirdiği doğrulanmalı.
        *   **[ ] Batching Gözlem Testi:** Birden çok tarayıcı ile eşzamanlı istekler gönderildiğinde, servis loglarında `Processing batch of size: 2` (veya daha üstü) mesajının görüldüğü teyit edilmeli.

---

## 🎯 PLANLANAN GÖREVLER (FAZ 2 & ÖTESİ - "Kaleyi Genişletme")

Bu faz, kararlı MVP temeli üzerine, Deepseek vizyonundaki gelişmiş özellikleri ve `governance` mimarisindeki hedefleri ekleyecektir.

-   **[ ] TASK ID: `UI-PRO-002` - Gelişmiş Panel Entegrasyonu**
    *   **Açıklama:** Sentiric Studio'ya, Deepseek vizyonundaki "Bağlamsal Bilgi" ve "Analiz" panellerinin daha gelişmiş versiyonlarını eklemek.
    *   **Vizyon:** Sağ panelde, sadece metrikler değil, aynı zamanda `knowledge-service`'ten gelen RAG sonuçlarını, `user-service`'ten gelen kullanıcı bilgilerini ve `cdr-service`'ten gelen geçmiş konuşma özetlerini gösterebilmek.

-   **[ ] TASK ID: `LLM-PERF-002` - Gerçek Paralel Batch Processing Implementasyonu**
    *   **Açıklama:** Mevcut kararlı ardışık batch işleme mantığını, `llama.cpp`'nin çoklu dizi (`multi-sequence`) decode yeteneklerini tam olarak kullanan gerçek bir paralel işleme mekanizmasıyla değiştirmek.
    *   **Vizyon:** Servisin saniye başına token (throughput) kapasitesini, özellikle yüksek VRAM'li sistemlerde en üst düzeye çıkarmak.

-   **[ ] TASK ID: `UI-PRO-003` - Çalışma Alanları ve Kalıcılık**
    *   **Açıklama:** Deepseek vizyonundaki "Sol Sidebar - Çalışma Alanları" özelliğini hayata geçirmek. Kullanıcıların sohbet geçmişlerini, prompt şablonlarını ve RAG context'lerini kaydedip yeniden kullanabilmelerini sağlamak.
    *   **Vizyon:** Bu özellik, `user-service` ve `dialplan-service`'in veritabanı ile entegrasyon gerektirecek ve Stüdyo'yu kişiselleştirilmiş bir geliştirme ortamına dönüştürecektir.

-   **[ ] TASK ID: `GW-ARC-001` - `llm-gateway-service` Mimarisine Geçiş**
    *   **Açıklama:** `governance`'da tanımlanan `llm-gateway-service`'i tasarlamak ve geliştirmek. `llm-llama-service`'i, bu gateway'in arkasında çalışan bir "uzman motor" olarak yeniden konumlandırmak.
    *   **Vizyon:** Bu, projenin nihai mimari hedefidir. Gateway, kimlik doğrulama, `tenant` bazlı model yönlendirme (örn: "Bu müşteri Gemini kullansın, diğeri yerel Llama") ve maliyet takibi gibi merkezi görevleri üstlenecektir.
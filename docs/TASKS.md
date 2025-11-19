# 📋 Sentiric LLM Llama Service - Görev ve Yol Haritası (Rev. 2)

---
## 🎯 PROJE VİZYONU VE KAPSAMI
Bu servis, Sentiric mimarisinin arkasındaki yüksek performanslı, state'siz bir LLM inference motorudur. Kendi `studio/` arayüzü, yalnızca servisin temel yeteneklerini test etmek ve sergilemek için bir geliştirici aracıdır. Tam kapsamlı kullanıcı arayüzü, `sentiric-studio-ui` projesinin sorumluluğundadır.

---
## ✅ TAMAMLANAN GÖREVLER (FAZ 1 - MVP Temeli)

-   **[✓] TASK ID: `LLM-BUILD-001` - Tekrarlanabilir Build Altyapısı Kurulumu**
    *   **Açıklama:** Proje, `vcpkg` bağımlılık yönetimi, `CMake` derleme sistemi ve çok aşamalı `Dockerfile`'lar (CPU/GPU) ile sağlam ve tekrarlanabilir bir build sürecine kavuşturuldu.
-   **[✓] TASK ID: `LLM-CORE-001` - Eşzamanlı İstek Mimarisi (Context Pool)**
    *   **Açıklama:** `LlamaContextPool` ve RAII tabanlı `ContextGuard` yapıları implemente edilerek, servisin birden çok isteği aynı anda ve birbirini engellemeden işleyebilmesi sağlandı.
-   **[✓] TASK ID: `LLM-STBL-001` - Kritik Bellek Hatalarının Giderilmesi (SegFault)**
    *   **Açıklama:** Servisin yük altında `exit code 139` (Segmentation Fault) ile çökmesine neden olan kritik bellek yönetimi hataları, RAII prensipleri ve güvenli kaynak temizleme mantığı ile tamamen çözüldü.
-   **[✓] TASK ID: `LLM-API-003` - OpenAI Uyumlu API Endpoint'i Entegrasyonu**
    *   **Açıklama:** `HttpServer`'a, endüstri standardı olan `/v1/chat/completions` endpoint'i eklendi. Bu endpoint, hem streaming hem de non-streaming modlarını desteklemektedir.
-   **[✓] TASK ID: `LLM-PERF-001` - Kararlı Dinamik Batching Mekanizması**
    *   **Açıklama:** `DynamicBatcher` mimarisi, gelen istekleri gruplayarak motorun daha verimli çalışmasını sağlayacak şekilde entegre edildi.
-   **[✓] TASK ID: `UI-PRO-001` - "Sentiric Studio" MVP Arayüzünün Geliştirilmesi**
    *   **Açıklama:** Projenin yeteneklerini sergilemek ve test etmek için Vue.js 3 tabanlı fonksiyonel bir "Stüdyo" arayüzü (`studio/`) geliştirildi.

---
## ⏳ ÜZERİNDE ÇALIŞILAN GÖREVLER (FAZ 2 - Çekirdek Motor İyileştirmesi)

-   **[ ] TASK ID: `LLM-VAL-001` - Faz 1 MVP'nin Kapsamlı Test ve Doğrulaması**
    *   **Açıklama:** Faz 1'in tüm temel yeteneklerinin (RAG, Konuşma Geçmişi, Eşzamanlılık, Batching) beklendiği gibi çalıştığını doğrulamak.
    *   **Kabul Kriterleri:**
        *   [ ] Temel Sohbet Testi
        *   [ ] Sistem Promptu Testi
        *   [ ] RAG Context Testi
        *   [ ] Parametre Testi
        *   [ ] Batching Gözlem Testi (Loglarda `Processing batch of size: >1` görülmeli)

-   **[ ] TASK ID: `LLM-PERF-002` - Gerçek Paralel Batch Processing Implementasyonu**
    *   **Açıklama:** Mevcut ardışık batch işleme mantığını, `llama.cpp`'nin çoklu dizi (`multi-sequence`) decode yeteneklerini kullanan gerçek bir paralel işleme mekanizmasıyla değiştirmek.
    *   **Hedef:** Servisin saniye başına token (throughput) kapasitesini en üst düzeye çıkarmak.

---
## 🎯 PLANLANAN GÖREVLER (FAZ 2 & Ötesi)

-   **[ ] TASK ID: `GW-ARC-001` - `llm-gateway-service` Mimarisine Geçiş Planlaması**
    *   **Açıklama:** Bu servisin, `llm-gateway-service`'in arkasında çalışan bir "uzman motor" olarak nasıl konumlandırılacağını planlamak.

---
## 🔗 HARİCİ BAĞIMLILIKLAR VE İSTEKLER
Bu bölüm, diğer Sentiric servislerinden beklenen ve bu servisin tam potansiyelini ortaya çıkaracak olan görevleri tanımlar.

-   **[ ] DEP-ID: `UI-PRO-001` -> Sorumlu Proje: `sentiric-studio-ui`**
    *   **İstek:** Deepseek vizyonundaki çoklu panel, yeniden boyutlandırılabilir, kalıcı çalışma alanları içeren profesyonel IDE'nin geliştirilmesi.
    *   **Gerekçe:** `llm-llama-service`'in gelişmiş yeteneklerinin son kullanıcıya sunulması.

-   **[ ] DEP-ID: `DB-SVC-001` -> Sorumlu Proje: `sentiric-persistence-service`**
    *   **İstek:** Sohbet geçmişlerini, kullanıcı ayarlarını ve çalışma alanı yapılandırmalarını saklayacak bir veritabanı servisinin sağlanması.
    *   **Gerekçe:** `sentiric-studio-ui`'nin stateful (kalıcı) özelliklerini desteklemek.
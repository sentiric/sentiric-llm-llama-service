# 💡 Çözülmüş Sorunlar Veritabanı (Knowledge Base)

Bu doküman, geliştirme sırasında karşılaşılan kritik sorunları ve kök neden analizlerini içerir.

---

### SORUN-005: 1B Modellerin RAG Başarısızlığı (Hallucination)

-   **Tarih:** 2025-12-15
-   **Belirtiler:** `Gemma 3 1B` ve `Llama 3.2 1B` modelleri, RAG ile verilen net bilgilere rağmen "Bilmiyorum" cevabı veriyor veya context dışı genel bilgiler uyduruyor. Matrix testlerinde `FAILED` durumu.
-   **Kök Neden:** 1 Milyar parametreli modellerin "Instruction Following" (Talimat Takibi) kapasitesi, karmaşık "Context + System Prompt + User Query" yapısını yönetmek için yetersiz kalıyor. Dikkat mekanizması (Attention Head) context'e yeterince odaklanamıyor.
-   **Çözüm:** Model mimarisi **Qwen 2.5 3B (Instruct)** olarak değiştirildi. 3B parametre sınıfı, hız ve zeka arasındaki "Altın Oran"ı (Sweet Spot) sağladı.
-   **Sonuç:** RAG testleri %100 başarıyla geçti.

---

### SORUN-006: Linker Hatası (Multiple Definition)

-   **Tarih:** 2025-12-15
-   **Belirtiler:** `make up` sırasında `multiple definition of 'SystemController::...'` hatası ile derleme kesildi.
-   **Kök Neden:** Refactoring sırasında `model_controller.cpp` dosyasının içine yanlışlıkla `SystemController` sınıfının implementasyonları kopyalanmış. Header guard'lar olsa bile `.cpp` dosyalarındaki implementasyon çakışması linker'ı bozdu.
-   **Çözüm:** `SystemController`, `ModelController` ve `ChatController` dosyaları **Single Responsibility Principle (SRP)** ilkesine göre tamamen ayrıştırıldı ve temizlendi.

---

### SORUN-007: Stress Testi Raporlama Hatası (Integer Underflow)

-   **Tarih:** 2025-12-15
-   **Belirtiler:** `stress-test.sh` raporunda "Toplam Token: 0" ve astronomik Yanıt Süresi (438208...) görülüyordu.
-   **Kök Neden:** `benchmark.cpp` içindeki `run_concurrent_test` fonksiyonunda, thread'ler arası veri paylaşımı sırasında `std::atomic` kullanılmadığı için Race Condition oluşuyordu ve sayaçlar doğru artmıyordu.
-   **Çözüm:** Tüm sayaçlar (`success_count`, `total_tokens`) `std::atomic` tipine dönüştürüldü.
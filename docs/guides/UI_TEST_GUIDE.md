# 🧪 Omni-Studio UI Test Yönergesi

Bu belge, `studio-v2` arayüzünün fonksiyonel ve görsel doğruluğunu test etmek için adımları içerir. Her release öncesi bu adımlar manuel olarak uygulanmalıdır.

## 1. Hazırlık
- Servisi başlatın: `make up`
- Tarayıcıyı açın: `http://localhost:16070`
- Console'u (F12) açın ve hataları izleyin.

## 2. Görsel ve Fonksiyonel Kontrol Listesi

### A. Header & Navigasyon
- [ ] **Model Seçici:** Dropdown açıldığında "Default", "Coder", "Reasoning" kategorileri görünüyor mu?
- [ ] **Durum Işığı:** Başlangıçta sarı (Loading), hazır olunca yeşil (Online) oluyor mu?
- [ ] **Model Değişimi:** Farklı bir model seçildiğinde "Model Yükleniyor" overlay'i çıkıyor mu?

### B. Sohbet Alanı (Chat)
- [ ] **Mesaj Gönderimi:** "Merhaba" yazıp Enter'a basınca mesaj baloncuğu oluşuyor mu?
- [ ] **Typing Indicator:** AI cevap vermeden önce "..." animasyonu görünüyor mu?
- [ ] **Stream Akışı:** Cevap tek parça değil, kelime kelime (streaming) akıyor mu?
- [ ] **Markdown Render:** Kalın, italik ve liste öğeleri düzgün görünüyor mu?

### C. Gelişmiş Özellikler (Advanced)
- [ ] **Chain of Thought (CoT):** Reasoning seviyesini "High" yapın. Cevap gelirken "Düşünce Süreci" kutusu çıkıyor mu? Tıklayınca açılıyor mu?
- [ ] **Artifacts:** AI'dan "Bir HTML butonu kodu yaz" isteyin. Sağ panelde kod veya önizleme otomatik açılıyor mu?
- [ ] **Stop Butonu:** Cevap yazılırken "Kare" (Stop) butonuna basınca üretim duruyor mu?

### D. Ayarlar Paneli (Sağ Panel)
- [ ] **Ayarları Aç:** Dişli çark ikonuna basınca sağ panel kayarak geliyor mu?
- [ ] **Slider Testi:** "Temperature" slider'ını oynatınca yandaki sayısal değer değişiyor mu?
- [ ] **Donanım Ayarı:** "GPU Layers"ı değiştirip "Uygula" dediğinizde sistem restart atıp (Overlay çıkar) geri geliyor mu?

### E. Telemetri Paneli
- [ ] **TPS Grafiği:** Cevap üretilirken grafik hareketleniyor mu?
- [ ] **Latency:** İlk token gelme süresi (Latency) mantıklı bir değer (ör: 500ms - 2000ms) gösteriyor mu?

## 3. Hata Senaryoları
- **Backend Kapalıyken:** Servisi durdurun (`make down`). UI'da "Offline" yazısı ve kırmızı nokta görülmeli.
- **Bozuk Dosya:** "Dosya Yükle" butonuna basıp geçersiz bir dosya seçince UI donmamalı.
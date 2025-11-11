# 📋 Geliştirme Görev Listesi

## 🚀 Yüksek Öncelik (High Priority)

### 1. Üretim Kalitesini ve Kontrolünü İyileştirme
*Bu, modelin anlamsız ve tekrar eden çıktılar üretmesini engellemek için **mevcut en kritik** görevdir.*
-   [ ] **Repetition Penalty (Tekrar Cezası) Ekle:** `LLMEngine` ve `llama.cpp` sampler zincirine tekrar eden token'ları cezalandırma mantığı ekle.
-   [ ] **Gelişmiş Sampling Parametreleri Ekle:**
    -   [ ] `temperature` sampling implementasyonu.
    -   [ ] `top_k` sampling implementasyonu.
    -   [ ] `top_p` (nucleus) sampling implementasyonu.
-   [ ] **gRPC API'sini Güncelle:** Yeni sampling parametrelerini (`temperature`, `top_k`, `top_p`, `repetition_penalty`) `GenerationParams` mesajına ekle.
-   [ ] **Prompt Template Uygula:** `LLMEngine`'de, kullanıcı prompt'unu `llama_chat_apply_template` kullanarak modelin beklediği instruct formatına dönüştür.

### 2. Performans Optimizasyonları
-   [ ] Batch decoding implementasyonu (birden çok sequence'i aynı anda işleme).
-   [ ] KV cache optimizasyonlarını araştır.
-   [ ] GPU offloading desteği ekle.

### 3. API ve Gözlemlenebilirlik
-   [ ] HTTP üzerinden `/generate` endpoint'i ekle.
-   [ ] Prometheus metrikleri için bir `/metrics` endpoint'i oluştur.
-   [ ] Temel istek hız limitleme (rate limiting) mekanizması ekle.

## 📊 Orta Öncelik (Medium Priority)

-   [ ] Yapılandırılmış (JSON) loglama.
-   [ ] YAML veya TOML tabanlı harici konfigürasyon dosyası desteği.
-   [ ] Dinamik model yükleme (servisi yeniden başlatmadan model değiştirme).
-   [ ] API için temel token bazlı authentication.

## 🔧 Düşük Öncelik (Low Priority)

-   [ ] Unit test kapsamını artır.
-   [ ] Kapsamlı entegrasyon testleri.
-   [ ] Model hot-swapping (kesintisiz model değiştirme).

---

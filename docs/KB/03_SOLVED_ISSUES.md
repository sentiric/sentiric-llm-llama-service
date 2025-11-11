# Çözülmüş Sorunlar Veritabanı

## SORUN-001: API Uyumsuzluğu Nedeniyle Derleme Hataları
- **Tarih:** 2025-11-11
- **Belirtiler:** `llama_kv_cache_clear`, `llama_sample_token_greedy` gibi fonksiyonlar bulunamadı.
- **Kök Neden:** Kod, `llama.cpp`'nin eski bir API'sini kullanırken, build süreci en güncel sürümü çekiyordu.
- **Çözüm:** `LLMEngine`, güncel `llama_sampler`, `llama_batch` ve context yönetimi API'lerini kullanacak şekilde yeniden yazıldı.

## SORUN-002: `libllama.so` / `libggml.so` Yüklenememe Hatası (Runtime)
- **Tarih:** 2025-11-11
- **Belirtiler:** Konteyner başlarken `cannot open shared object file` hatası vererek çöküyordu.
- **Kök Neden:** Dockerfile, derlenen `.so` paylaşılan kütüphanelerini `builder` aşamasından `runtime` aşamasına kopyalamıyordu.
- **Çözüm:** Dockerfile'a `/app/build/bin/*.so` dosyalarını `/usr/local/lib/` dizinine kopyalayan ve `ldconfig` çalıştıran komutlar eklendi.
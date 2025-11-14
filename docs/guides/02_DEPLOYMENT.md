# 🚀 Dağıtım Rehberi

Bu servis, esnek bir `docker-compose` yapısı kullanarak farklı senaryolarda kolayca dağıtılabilir. Bu rehber, hem üretim (pre-built imajları kullanarak) hem de geliştirme (kaynaktan derleyerek) ortamları için adımları açıklar.

## 1. Mimari Yaklaşımı: Temel + Profil + Geçersiz Kılma

Yapılandırmayı basitleştirmek için aşağıdaki mimariyi kullanıyoruz:
- **`docker-compose.yml`:** Tüm ortak yapılandırmaları içeren temel dosyadır.
- **`docker-compose.cpu.yml` / `docker-compose.gpu.yml`:** Sadece CPU veya GPU'ya özel farkları (imaj adı, kaynaklar) tanımlayan "profil" dosyalarıdır.
- **`docker-compose.override.yml` / `docker-compose.gpu.override.yml`:** Sadece yerel geliştirme için kaynaktan derleme (`build`) talimatlarını içeren "geçersiz kılma" dosyalarıdır.

---

## 2. Üretim Dağıtımı (Pre-built İmajları Çekerek)

Bu senaryo, GitHub Container Registry'den (ghcr.io) hazır imajları çeker.

### 2.1. CPU Üzerinde Çalıştırma

```bash
# Temel ve CPU profili dosyalarını kullanarak servisi başlat
docker compose -f docker-compose.yml -f docker-compose.cpu.yml up -d
```

### 2.2. GPU Üzerinde Çalıştırma (NVIDIA)

```bash
# Temel ve GPU profili dosyalarını kullanarak servisi başlat
docker compose -f docker-compose.yml -f docker-compose.gpu.yml up -d
```

---

## 3. Geliştirme Ortamı (Kaynaktan Derleyerek)

Bu senaryo, yerel kod değişikliklerinizi test etmek için kullanılır.

### 3.1. CPU Üzerinde Derleme ve Çalıştırma

`docker-compose.override.yml` dosyası, `docker compose` tarafından otomatik olarak algılanır ve temel `docker-compose.yml`'i ezer.

```bash
# Bu komut, Dockerfile kullanarak yerel bir imaj oluşturur ve servisi başlatır
docker compose up --build -d
```

### 3.2. GPU Üzerinde Derleme ve Çalıştırma (NVIDIA)

GPU derlemesi için geçersiz kılma dosyasını manuel olarak belirtmemiz gerekir.

```bash
# Temel, GPU profili ve GPU geçersiz kılma dosyalarını birleştirerek servisi başlat
docker compose -f docker-compose.yml -f docker-compose.gpu.yml -f docker-compose.gpu.override.yml up --build -d
```

---

## 4. Servisi Durdurma ve Temizleme

Hangi profille başlattığınızdan bağımsız olarak, servisi durdurmak için:
```bash
# Konteynerleri durdur ve kaldır
docker compose down

# Model ve diğer volümleri temizlemek için (opsiyonel):
docker compose down -v
```

---

## 5. Yapılandırma (Configuration)

Servisin tüm yapılandırma seçenekleri, ortam değişkenleri aracılığıyla yönetilir. Detaylı referans için lütfen aşağıdaki belgeyi inceleyin:

- **[Yapılandırma Rehberi](./03_CONFIGURATION.md)**

---

## 6. Kaynak Gereksinimleri

Bu servis, `LlamaContextPool` mimarisi sayesinde gerçek eşzamanlılık sunar. Ancak bu, kaynak kullanımı üzerinde doğrudan bir etkiye sahiptir.

**Gerekli Toplam Bellek ≈ Model Boyutu + ( `LLM_LLAMA_SERVICE_THREADS` × Her Context için KV Cache Boyutu )**

- **Model Boyutu:** Kullandığınız GGUF dosyasının boyutu.
- **KV Cache Boyutu:** Bu, `CONTEXT_SIZE`'a bağlıdır. Örneğin, `phi-3-mini-4k-instruct-q4.gguf` için 4096 context ile yaklaşık **1.5 GB**'tır.

**Örnek:** `LLM_LLAMA_SERVICE_THREADS=3` ayarıyla, en az `Model Boyutu + 4.5 GB` RAM/VRAM gereklidir. Kaynaklarınızı bu ihtiyaca göre planlayın. Yetersiz kaynak, `out of memory` hatalarına veya sistemin yavaşlamasına neden olur.


---

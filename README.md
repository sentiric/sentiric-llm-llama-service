# 🧠 Sentiric LLM Llama Service

**Production-ready** yerel LLM servisi - C++ ile yüksek performanslı AI motoru. Phi-3-mini modeli ile tam entegre.

## 🚀 Özellikler

- ✅ **Yüksek Performans**: C++ & llama.cpp optimizasyonu
- ✅ **GRPC Streaming**: Token-token real-time yanıt  
- ✅ **HTTP Health Check**: `/health` endpoint
- ✅ **Docker Container**: Tam izole edilmiş deployment
- ✅ **Stable Build**: Static linking ile güvenilir çalışma
- ✅ **Phi-3-mini Model**: 3B parametre, Türkçe destek
- ✅ **Hafif Repo**: Submodulesüz, temiz yapı

## 📦 Teknik Spesifikasyonlar

### Versiyon Bilgisi
- **Servis Versiyonu**: v1.0.0-stable
- **llama.cpp Commit**: 0750a59903688746883b0ecb24ac5ceed68edbf1
- **Model**: Phi-3-mini-4k-instruct-q4.gguf
- **Bağımlılıklar**: Static build (libgomp1 hariç)

### Port Yapılandırması
- **HTTP Health**: 16060
- **GRPC Service**: 16061

### Model Özellikleri
- **Boyut**: 2.23GB (Q4_K quantize)
- **Context**: 4096 token
- **Parametre**: 3.82B
- **Dil**: Türkçe & İngilizce

## 🛠️ Kurulum

### Ön Koşullar
- Docker & Docker Compose
- 4GB+ RAM
- 3GB+ Disk alanı

### Hızlı Başlangıç
```bash
# 1. Repoyu klonla (--recursive gerekmez!)
git clone https://github.com/sentiric/sentiric-llm-llama-service.git
cd sentiric-llm-llama-service

# 2. Modeli indir
./models/download.sh

# 3. Servisi başlat
docker compose up --build -d

# 4. Sağlık kontrolü
curl http://localhost:16060/health

# 5. Test
docker compose exec llm-llama-service grpc_test_client "Merhaba"
```

## 🔧 Geliştirici Rehberi

### Build Süreci
```bash
# Clean build
docker compose down
docker system prune -f
docker compose up --build -d
```

### Debug
```bash
# Logları izle
docker logs -f llm-llama-service

# Container'a bağlan
docker exec -it llm-llama-service bash
```

## 🎯 API Kullanımı

### GRPC İstemcisi
```cpp
// Örnek kullanım
auto client = LlamaClient(grpc::CreateChannel(
    "localhost:16061", 
    grpc::InsecureChannelCredentials()
));
client.GenerateStream("Türkiye'nin başkenti?");
```

### Health Endpoint
```bash
curl http://localhost:16060/health
# {"engine":"llama.cpp","model_ready":true,"status":"healthy"}
```

## 🐛 Sorun Giderme

### Sık Karşılaşılan Sorunlar

1. **libgomp.so.1 hatası**: 
   ```dockerfile
   # Çözüm: libgomp1 paketini yükle
   RUN apt-get install -y libgomp1
   ```

2. **Model yüklenemiyor**:
   - Model dosyasını kontrol et: `/models/phi-3-mini.q4.gguf`
   - Disk alanını kontrol et

3. **Build başarısız**:
   - Cache'i temizle: `docker system prune -f`
   - Static build flag'lerini kontrol et

## 📊 Performans

- **Model Yükleme**: ~30 saniye
- **Token Generation**: ~50 token/saniye
- **Bellek Kullanımı**: ~2.5GB
- **CPU Kullanımı**: 4 thread

## 🤝 Katkıda Bulunma

1. Fork yapın
2. Feature branch oluşturun
3. Değişiklikleri commit edin
4. Pull request açın

**ÖNEMLİ**: Static build flag'lerini değiştirmeyin!
# 🧠 Sentiric LLM Llama Service

**Sentiric LLM Llama Service**, yerel donanım üzerinde (on-premise) Büyük Dil Modeli (LLM) çıkarımı sağlayan, C++ ile yazılmış yüksek performanslı bir uzman AI motorudur. `llama.cpp` kütüphanesini temel alarak, popüler açık kaynaklı modelleri GGUF formatında çalıştırır.

Bu servis, `sentiric-contracts` v1.10.0+ API standardını uygular.

## 🎯 Temel Sorumluluklar
- **Maksimum Performans:** C++ ve `llama.cpp` ile donanıma en yakın hızda LLM çıkarımı.
- **Düşük Kaynak Tüketimi:** Kuantize edilmiş GGUF modelleri ile düşük bellek kullanımı.
- **gRPC Streaming:** Metin yanıtlarını token token üreterek düşük algılanan gecikme.
- **Dinamik Sağlık Kontrolü:** Modelin hazır olup olmadığını bildiren `/health` endpoint'i.

## 🛠️ Derleme ve Çalıştırma

### Ön Koşullar
- Docker ve Docker Compose
- Git
- Bir adet GGUF formatında LLM modeli (Örn: `phi-3-mini-4k-instruct.Q4_K_M.gguf`)

### Adım Adım Kurulum

1.  **Repoyu Klonla ve Submodule'leri Yükle:**
    ```bash
    git clone --recurse-submodules https://github.com/sentiric/llm-llama-service.git
    cd llm-llama-service
    ```

2.  **Modeli Hazırla:**
    - Proje kök dizininde `models` adında bir klasör oluşturun.
    - İndirdiğiniz GGUF model dosyasını bu klasörün içine kopyalayın. Örnek:
    ```bash
    ./models/download.sh
    ```

3.  **Yapılandırmayı Düzenle (Gerekirse):**
    - `docker-compose.yml` dosyasındaki `LLM_LOCAL_SERVICE_MODEL_PATH` değişkenini, kendi model dosyanızın adıyla güncelleyin.

4.  **Servisi Başlat:**
    ```bash
    docker compose up --build
    ```
    İlk derleme birkaç dakika sürebilir.

## ✅ Doğrulama

-   **Sağlık Kontrolü:** `curl http://localhost:16060/health` komutunu çalıştırın. `{"model_ready":true}` yanıtını görmelisiniz.
-   **gRPC Test:** Servis çalışırken, **ayrı bir terminalde** aşağıdaki komutu çalıştırın.
    ```bash
    # Test istemcisini çalıştır
    docker compose exec llm-llama-service grpc_test_client "Türkiye'nin başkenti neresidir?"
    ```
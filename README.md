# 🧠 Sentiric LLM Llama Service

**Sentiric LLM Llama Service**, yerel donanım üzerinde (on-premise) Büyük Dil Modeli (LLM) çıkarımı sağlayan, C++ ile yazılmış yüksek performanslı bir uzman AI motorudur. `llama.cpp` kütüphanesini temel alarak, `Phi-3`, `Llama3` gibi popüler açık kaynaklı modelleri GGUF formatında, minimum kaynak tüketimi ve gecikme ile çalıştırır.

Bu servis, `llm-gateway-service` tarafından, en üst düzeyde performans, güvenlik ve verimlilik gerektiren metin üretimi ihtiyaçları için çağrılır. Bu servis, Python tabanlı `llm-local-service`'in yerini alacak şekilde tasarlanmıştır.

## 🎯 Temel Sorumluluklar

-   **Maksimum Performans:** C++ ve `llama.cpp` sayesinde Python ek yükü (overhead) olmadan, donanıma en yakın hızda LLM çıkarımı yapar.
-   **Düşük Kaynak Tüketimi:** Kuantize edilmiş GGUF modelleri ile çok düşük bellek (RAM) kullanımı sağlar.
-   **gRPC Streaming:** Metin yanıtlarını token token üreterek düşük algılanan gecikme sağlar.
-   **Donanım Verimliliği:** Modern CPU komut setlerini (AVX, AVX2) ve opsiyonel olarak GPU hızlandırmayı (CUDA, Metal) destekler.
-   **Dinamik Sağlık Kontrolü:** Modelin yüklenip hazır olup olmadığını bildiren `/health` endpoint'i.

## 🛠️ Derleme ve Çalıştırma

### Ön Koşullar
- Docker ve Docker Compose
- Git
- Bir adet GGUF formatında LLM modeli (Örn: `Phi-3-mini-4k-instruct.Q4_K_M.gguf`)
(https://huggingface.co/microsoft/Phi-3-mini-4k-instruct-gguf/resolve/main/Phi-3-mini-4k-instruct-q4.gguf)
### Adım Adım Kurulum

1.  **Repoyu Klonla:**
    ```bash
    git clone https://github.com/sentiric/llm-llama-service.git
    cd llm-llama-service
    ```

2.  **`llama.cpp` Submodule'ünü Yükle:**
    ```bash
    git submodule update --init --recursive
    ```

3.  **Modeli Hazırla:**
    - Proje kök dizininde `model-cache` adında bir klasör oluşturun.
    - İndirdiğiniz GGUF model dosyasını bu klasörün içine kopyalayın.

4.  **Yapılandırmayı Düzenle:**
    - `docker-compose.yml` dosyasını açın.
    - `environment` bölümündeki `LLM_LOCAL_SERVICE_MODEL_PATH` değişkenini, kendi model dosyanızın adıyla güncelleyin.

5.  **Servisi Başlat:**
    ```bash
    docker compose up --build
    ```
    İlk derleme işlemi birkaç dakika sürebilir. Sonraki başlatmalar çok daha hızlı olacaktır.

## ✅ Doğrulama

-   **Sağlık Kontrolü:** Tarayıcınızda `http://localhost:16060/health` adresini açın veya terminalden `curl http://localhost:16060/health` komutunu çalıştırın. `{"model_ready":true}` yanıtını görmelisiniz.
-   **gRPC Test:** Projeyle birlikte gelen Python test istemcisini kullanarak servisi test edin:
    ```bash
    # Gerekli kütüphaneleri kurun
    pip install grpcio protobuf

    # Test istemcisini çalıştırın
    python grpc_test_client.py "Türkiye'nin başkenti neresidir?"
    ```
# 🏗️ Teknik Mimari

## 1. Katmanlı Bağımlılık Mimarisi

Sistem, derleme sürelerini optimize etmek ve bağımlılıkları modülerleştirmek için katmanlı bir Docker imaj yapısı kullanır.

```
┌───────────────────────────┐
│ ghcr.io/sentiric/vcpkg-base │ (Build Tools, vcpkg)
└─────────────┬─────────────┘
              │
┌─────────────▼─────────────┐
│ ghcr.io/sentiric/llama-cpp  │ (libllama.so, Headers)
└─────────────┬─────────────┘
              │
┌─────────────▼─────────────┐
│ sentiric-llm-llama-service  │ (Application Logic)
└───────────────────────────┘
```

## 2. Sistem Diagramı

Servis, bir model motoru ve iki sunucu arayüzünden oluşur. Eşzamanlı istekler, bir `LlamaContextPool` tarafından yönetilir.

```
                                    ┌──────────────────┐
                                ┌───►   gRPC Request   │
                                │   └──────────────────┘
   ┌─────────────┐    gRPC/HTTP   │   ┌──────────────────┐
   │   Clients   │◄──────────────┼───►   gRPC Request   │
   │ (llm_cli)   │              │   └──────────────────┘
   └─────────────┘              │   ┌──────────────────┐
                                └───►   gRPC Request   │
                                    └──────────────────┘
                                              │
                                     ┌────────▼─────────┐
                                     │  LLM Service     │
                                     │ ┌──────────────┐ │
                                     │ │  gRPC Server │ │
                                     │ ├──────────────┤ │
                                     │ │  HTTP Server │ │
                                     │ ├──────────────┤ │
                                     │ │  LLM Engine  │ │
                                     │ └──────┬───────┘ │
                                     └────────┼─────────┘
                                              │
                                     ┌────────▼─────────┐
                                     │ LlamaContextPool │
                                     │ ┌──────────────┐ │
                                     │ │ llama_context│ │
                                     │ │ llama_context│ │
                                     │ │ ... (N adet) │ │
                                     │ └──────────────┘ │
                                     └────────┬─────────┘
                                       ┌──────▼──────┐
                                       │ libllama.so │ (Shared Library)
                                       └──────┬──────┘
                                       ┌──────▼──────┐
                                       │  Phi-3 Model  │
                                       └─────────────┘
```

## 3. Eşzamanlılık Modeli (Concurrency)

Önceki mimarideki global `std::mutex` darboğazı giderilmiştir. Yeni mimari, bir **context havuzu (`LlamaContextPool`)** kullanır:

-   Servis başladığında, `n_threads` sayısı kadar `llama_context` oluşturulur ve havuza eklenir.
-   Her gelen gRPC isteği, havuzdan boşta bir `llama_context` talep eder.
-   İstek, bu context'i kullanarak token üretme işlemini gerçekleştirir. Bu sırada diğer istekler, havuzdaki diğer boş context'leri kullanarak paralel olarak işlenebilir.
-   İşlem bittiğinde, context temizlenir (KV cache sıfırlanır) ve tekrar havuza bırakılır.

Bu yapı, servisin CPU kaynaklarını tam olarak kullanarak **gerçek eşzamanlılık** sağlar.

## 4. Build Süreci

-   **CMake:** `find_package` kullanarak bağımlılıkları (gRPC, llama, spdlog vb.) modern ve taşınabilir bir şekilde bulur.
-   **FetchContent:** `sentiric-contracts` reposunu derleme anında çeker ve proto dosyalarını işler.
-   **Dockerfile:** Multi-stage build kullanır. `builder` aşamasında tüm derlemeler yapılır. `runtime` aşamasına ise sadece çalıştırılabilir dosyalar ve gerekli paylaşılan kütüphaneler (`libllama.so`, `libgomp1.so`) kopyalanır.


---

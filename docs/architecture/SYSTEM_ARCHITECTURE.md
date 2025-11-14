# 🏗️ Sistem Mimarisi (Rev. 2)

## 1. Sistem Diyagramı

Servis, gelen istemci isteklerini işleyen, bir model motoru aracılığıyla token üreten ve bu token'ları stream eden bir yapıdır. Eşzamanlılık, bir `LlamaContextPool` tarafından yönetilir.

```mermaid
graph TD
    subgraph Clients
        A[llm_cli]
        B[Python Client]
        C[llm-gateway]
    end

    subgraph LLM Service Container
        direction LR
        subgraph "API Endpoints"
            gRPC_Server[gRPC Server (mTLS)]
            HTTP_Server[HTTP Server (Health)]
        end

        LLM_Engine[LLM Engine]
        
        subgraph "Concurrency Management"
            LlamaContextPool(Llama Context Pool)
        end

        gRPC_Server --> LLM_Engine
        HTTP_Server --> LLM_Engine
        LLM_Engine --> LlamaContextPool
    end

    subgraph "llama.cpp Backend"
        libllama[libllama.so + common]
        ModelFile[(GGUF Model)]
    end
    
    Clients -- gRPC / HTTP --> LLM Service Container
    LlamaContextPool -- "Acquires/Releases Context" --> libllama
    libllama -- "Loads/Interacts" --> ModelFile

    classDef client fill:#d4edda,stroke:#155724
    classDef service fill:#cce5ff,stroke:#004085
    classDef backend fill:#f8d7da,stroke:#721c24
    
    class A,B,C client
    class gRPC_Server,HTTP_Server,LLM_Engine,LlamaContextPool service
    class libllama,ModelFile backend
```

## 2. Eşzamanlılık Modeli (Concurrency)

Mimari, bir **context havuzu (`LlamaContextPool`)** kullanarak gerçek eşzamanlılık sağlar. Bu, servisin en kritik performans bileşenidir.

-   **İlklendirme:** Servis başladığında, `LLM_LLAMA_SERVICE_THREADS` ortam değişkeni ile belirlenen sayıda `llama_context` oluşturulur ve havuza eklenir.
-   **İstek İşleme:** Her gelen gRPC isteği, havuzdan boşta bir `llama_context` "kiralar". Bu sırada diğer istekler, havuzdaki diğer boş context'leri kullanarak **paralel olarak** işlenir.
-   **Kaynak İadesi ve Temizlik (Kritik Adım):** İşlem bittiğinde veya istemci bağlantıyı kapattığında, kullanılan context'in KV cache'i `llama_kv_cache_seq_rm(ctx, -1, 0, -1)` çağrısı ile **mutlaka temizlenir** ve context tekrar havuza bırakılır. Bu, bir sonraki isteğin, önceki isteğin "hafızası" olmadan temiz bir state ile başlamasını garanti eder. Bu adımın atlanması, state sızıntısına (state leak) ve hatalı çıktılara yol açar.

## 3. Build ve Bağımlılık Mimarisi

Sistem, bağımlılıkları derleme anında çözümleyen, taşınabilir ve kendi kendine yeten (self-contained) bir Docker imaj yapısı kullanır.

1.  **vcpkg Kurulumu:** `vcpkg` paket yöneticisi, `vcpkg.json` dosyasında belirtilen C++ kütüphanelerini (`gRPC`, `spdlog` vb.) derler.
2.  **`llama.cpp` Klonlama:** `Dockerfile` içinde belirtilen **sabit bir commit hash'i** kullanılarak `ggerganov/llama.cpp` reposu klonlanır. Bu, tekrarlanabilir ve stabil build'leri garanti eder.
3.  **Uygulama Derlemesi:** Projenin ana kodu (`llm_service`, `llm_cli`), `vcpkg` kütüphanelerine ve anlık derlenen `llama` ve `common` kütüphanelerine karşı derlenir. `CMakeLists.txt`, `LLAMA_BUILD_COMMON=ON` bayrağını ayarlayarak `common` kütüphanesinin derlenmesini zorunlu kılar.
4.  **Runtime İmajı:** Minimal bir `ubuntu` veya `nvidia/cuda` runtime imajı üzerine sadece çalıştırılabilir dosyalar ve `llama.cpp`'nin gerektirdiği paylaşılan kütüphaneler (`*.so`) kopyalanır ve `ldconfig` ile linklenir.

---

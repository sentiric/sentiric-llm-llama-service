# 🏗️ Sistem Mimarisi

## 1. Sistem Diyagramı

Servis, gelen istemci isteklerini işleyen, bir model motoru aracılığıyla token üreten ve bu token'ları stream eden bir yapıdır. Eşzamanlılık, bir `LlamaContextPool` tarafından yönetilir.

```mermaid
graph TD
    subgraph Clients
        A[llm_cli]
        B[Python Client]
        C[...]
    end

    subgraph LLM Service Container
        direction LR
        subgraph "API Endpoints"
            gRPC_Server[gRPC Server]
            HTTP_Server[HTTP Server]
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
        libllama[libllama.so + deps]
        ModelFile[(Phi-3 GGUF Model)]
    end
    
    Clients -- gRPC / HTTP --> LLM Service Container
    LlamaContextPool -- "Acquires/Releases" --> libllama
    libllama -- "Loads/Interacts" --> ModelFile

    classDef client fill:#d4edda,stroke:#155724
    classDef service fill:#cce5ff,stroke:#004085
    classDef backend fill:#f8d7da,stroke:#721c24
    
    class A,B,C client
    class gRPC_Server,HTTP_Server,LLM_Engine,LlamaContextPool service
    class libllama,ModelFile backend
```

## 2. Eşzamanlılık Modeli (Concurrency)

Mimari, bir **context havuzu (`LlamaContextPool`)** kullanarak gerçek eşzamanlılık sağlar:

-   Servis başladığında, `LLM_THREADS` sayısı kadar `llama_context` oluşturulur ve havuza eklenir.
-   Her gelen gRPC isteği, havuzdan boşta bir `llama_context` talep eder.
-   İstek, bu context'i kullanarak token üretme işlemini gerçekleştirir. Bu sırada diğer istekler, havuzdaki diğer boş context'leri kullanarak paralel olarak işlenebilir.
-   İşlem bittiğinde, context'in KV cache'i `llama_memory_seq_rm` ile temizlenir ve tekrar havuza bırakılır. Bu, bir sonraki isteğin temiz bir state ile başlamasını garanti eder.

## 3. Build ve Bağımlılık Mimarisi

Sistem, bağımlılıkları derleme anında çözümleyen, taşınabilir ve kendi kendine yeten (self-contained) bir Docker imaj yapısı kullanır.

1.  **vcpkg Kurulumu:** `vcpkg` paket yöneticisi, `vcpkg.json` dosyasında belirtilen C++ kütüphanelerini (`gRPC`, `spdlog` vb.) derler.
2.  **`llama.cpp` Klonlama:** `ggerganov/llama.cpp` reposunun en güncel `master` branch'i, derleme ortamına klonlanır.
3.  **Uygulama Derlemesi:** Projenin ana kodu, `vcpkg` ve anlık derlenen `llama.cpp` kütüphanelerine karşı derlenir.
4.  **Runtime İmajı:** Minimal bir Ubuntu imajı üzerine sadece çalıştırılabilir dosyalar ve `llama.cpp`'nin gerektirdiği paylaşılan kütüphaneler (`*.so`) kopyalanır ve `ldconfig` ile linklenir.


---

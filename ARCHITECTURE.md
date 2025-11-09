# 🏗️ Teknik Mimari

## YENİ: Static Build Architecture

### Build Time Dependencies
```
┌─────────────────┐
│   Host System   │
│  - git          │
│  - cmake        │  
│  - docker       │
└─────────────────┘
         │
┌─────────────────┐
│   Builder       │
│  - vcpkg        │
│  - llama.cpp    │
│  - protobuf     │
└─────────────────┘
         │
┌─────────────────┐
│   Runtime       │
│  - libgomp1     │ ← TEK RUNTIME DEPENDENCY
└─────────────────┘
```

### Memory Optimization
- **Model Weights**: MMAP ile lazy loading (~2.23GB)
- **KV Cache**: 1536MB sabit allocation  
- **Context**: 4096 token capacity
- **Total**: ~2.5GB optimizasyonu

## Sistem Diagramı
```
┌─────────────────┐    GRPC    ┌──────────────────┐
│   GRPC Client   │ ◄─────────►│  LLM Service     │
└─────────────────┘  Streaming └──────────────────┘
                               │                  │
┌─────────────────┐    HTTP    │  - LLM Engine    │
│   HTTP Health   │ ◄─────────►│  - GRPC Server   │
│     Checker     │            │  - HTTP Server   │
└─────────────────┘            └──────────────────┘
                                      │
                               ┌──────▼──────┐
                               │ llama.cpp   │
                               │   Library   │
                               └──────┬──────┘
                               ┌──────▼──────┐
                               │ Phi-3-mini  │
                               │   Model     │
                               └─────────────┘
```

## Bileşenler

### 1. LLM Engine (`src/llm_engine.cpp`)
- **Model Yükleme**: `llama_model_load_from_file()`
- **Context Management**: `llama_init_from_model()`
- **Token Generation**: Greedy sampling + streaming
- **Thread Safety**: `std::mutex` ile thread-safe

### 2. GRPC Server (`src/grpc_server.cpp`)
- **Protocol**: sentiric-contracts v1.10.0
- **Streaming**: Real-time token delivery
- **Error Handling**: Graceful client disconnect

### 3. HTTP Server (`src/http_server.cpp`)
- **Health Check**: Model status monitoring
- **REST API**: JSON responses
- **Port**: 16060

## Build Süreci

### Static Build Zorunlulukları
```cmake
set(LLAMA_STATIC ON)
set(BUILD_SHARED_LIBS OFF)
target_link_libraries(llm_service PRIVATE llama)
```

### Docker Multi-stage
1. **Builder Stage**: Tüm bağımlılıklar + derleme
2. **Runtime Stage**: Sadece executable + libgomp1

## Data Flow

1. **GRPC Request** → `LocalGenerateStreamRequest`
2. **Tokenization** → `llama_tokenize()`
3. **Decoding** → `llama_decode()`
4. **Sampling** → Greedy selection
5. **Streaming** → Token-by-token response

## Güvenlik

- **No Network Access**: Tamamen yerel
- **Container Isolation**: Docker sandbox
- **Static Binary**: Minimal attack surface
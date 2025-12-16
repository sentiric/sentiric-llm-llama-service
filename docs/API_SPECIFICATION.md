# 📡 API Spesifikasyonu (v2.5)

## 1. gRPC Servisi: `LLMLocalService`

Telefon asistanı ve Gateway iletişimi için ana kanaldır.

### 1.1. `GenerateStream`
Metin üretimi için kullanılır.

**Metadata (Headers):**
-   `x-trace-id`: (Zorunlu değil ama önerilir) İsteğin yaşam döngüsünü izlemek için UUID. Loglarda bu ID ile arama yapılabilir.

**İstek (`GenerateStreamRequest`):**
```protobuf
message GenerateStreamRequest {
  string system_prompt = 1; // Opsiyonel. Asistan kimliği.
  string user_prompt = 2;   // Zorunlu. Kullanıcı sorusu.
  
  // [KRİTİK] RAG Verisi buraya gelir.
  // Doküman servisinden çekilen metin buraya ham string olarak basılır.
  optional string rag_context = 3; 
  
  repeated ConversationTurn history = 4; // Konuşma geçmişi.
  optional GenerationParams params = 5;  // Sıcaklık, Max Token vb.
}
```

## 2. HTTP Endpoint'leri (Yönetim ve UI)

### 2.1. OpenAI Uyumlu Chat Endpoint'i (`POST /v1/chat/completions`)
Stream ve unary (tek seferde) yanıtları destekleyen ana metin üretim endpoint'i.

**İstek Gövdesi (JSON):**
```json
{
  "messages": [
    { "role": "system", "content": "Sen bir asistansın." },
    { "role": "user", "content": "Merhaba, nasılsın?" }
  ],
  "rag_context": "Müşteri: Ayşe Yılmaz. Bakiye: 500 TL.", // [YENİ] RAG için bu alan kullanılır
  "temperature": 0.7,
  "max_tokens": 1024,
  "stream": false
}
```

### 2.2. Donanım Yapılandırması (`POST /v1/hardware/config`)
Servisi yeniden başlatmadan donanım ayarlarını günceller (Model reload tetikler).

**İstek:**
```json
{
  "gpu_layers": 100,
  "context_size": 8192,
  "kv_offload": true
}
```

### 2.3. Sağlık Durumu (`GET /health`)
Gateway ve Load Balancer için durum bilgisi.

**Yanıt:**
```json
{
  "status": "healthy",
  "model_ready": true,
  "current_profile": "qwen25_3b_phone_assistant",
  "capacity": {
    "active": 1,
    "total": 8,
    "available": 7
  }
}
```

---

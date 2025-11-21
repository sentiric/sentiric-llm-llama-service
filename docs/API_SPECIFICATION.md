# 📡 API Spesifikasyonu (v2.1 - OpenAI Uyumlu)

Bu belge, `llm-llama-service`'in sunduğu gRPC ve HTTP arayüzlerini tanımlar. Servis, endüstri standardı **OpenAI API** formatını destekler.

## 1. gRPC Servisi: `LLMLocalService`

Bu servis, `sentiric-contracts v1.11.0` ile tanımlanmıştır ve akış tabanlı, düşük gecikmeli diyalog yönetimi için tasarlanmıştır.

### 1.1. Servis Tanımı
```protobuf
service LLMLocalService {
  // Verilen diyalog bağlamına göre token-token metin üretir.
  rpc GenerateStream(LLMLocalServiceGenerateStreamRequest) 
      returns (stream LLMLocalServiceGenerateStreamResponse);
}
```

### 1.2. Ana Mesaj Tipleri
```protobuf
// İstek Mesajı
message LLMLocalServiceGenerateStreamRequest {
  // AI'nın genel kişiliğini ve kurallarını belirleyen ana talimat.
  string system_prompt = 1;

  // Kullanıcının son söylediği cümle veya sorduğu soru.
  string user_prompt = 2;

  // (Opsiyonel) RAG için kullanılan ek bilgi metni.
  optional string rag_context = 3;

  // (Opsiyonel) Konuşmanın geçmişi.
  repeated ConversationTurn history = 4;

  // (Opsiyonel) Token üretme ayarlarını geçersiz kılmak için.
  optional GenerationParams params = 5;
}

// Yanıt Mesajı
message LLMLocalServiceGenerateStreamResponse {
  oneof type {
    string token = 1;
    FinishDetails finish_details = 2;
  }
}
```
*Not: `ConversationTurn`, `GenerationParams` ve `FinishDetails` gibi yardımcı mesajların detayları için `sentiric-contracts` reposuna bakınız.*

## 2. HTTP Endpoint'leri (OpenAI Uyumlu)

Servis, standart OpenAI istemcileri (WebUI, LangChain vb.) ile entegrasyon için aşağıdaki endpoint'leri sunar.

### 2.1. Model Listesi (`GET /v1/models`)
Mevcut aktif modeli döndürür. Gateway ve Client Discovery için kullanılır.

```json
{
  "object": "list",
  "data": [
    {
      "id": "ggml-org/gemma-3-1b-it-qat-GGUF",
      "object": "model",
      "created": 1763763803,
      "owned_by": "sentiric-llm-service"
    }
  ]
}
```

### 2.2. Chat Completions (`POST /v1/chat/completions`)
Metin üretimi için ana endpoint. Streaming (SSE) destekler.

**İstek:**
```json
{
  "model": "gemma-3",
  "messages": [
    {"role": "system", "content": "Sen yardımsever bir asistansın."},
    {"role": "user", "content": "Merhaba!"}
  ],
  "stream": true,
  "temperature": 0.8,
  "max_tokens": 1024,
  "response_format": { "type": "json_object" } // Opsiyonel: JSON Modu için
}
```

**Grammar Desteği (Gelişmiş):**
OpenAI standardına ek olarak, `grammar` alanı ile saf GBNF string'i gönderilebilir.

### 2.3. Sağlık Kontrolü (`GET /health`)
```http
GET /health

Response (Başarılı):
Status: 200 OK
{
  "status": "healthy",
  "model_ready": true,
  "engine": "llama.cpp"
}

Response (Model Yükleniyor):
Status: 503 Service Unavailable
{
  "status": "unhealthy",
  "model_ready": false,
  "engine": "llama.cpp"
}
```

## 3. Hata Kodları

*   **gRPC:** `UNAVAILABLE` (14) - Model hazır değil, `INVALID_ARGUMENT` (3) - Gerekli alanlar eksik, `INTERNAL` (13) - Beklenmedik motor hatası.
*   **HTTP:** `200 OK` - Sağlıklı, `503 Service Unavailable` - Model hazır değil, `400 Bad Request` - Hatalı JSON veya parametre.

---

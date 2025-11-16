# 📡 API Spesifikasyonu (v2.0 - Diyalog Odaklı)

Bu belge, `llm-llama-service`'in sunduğu gRPC ve HTTP arayüzlerini tanımlar.

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

## 2. HTTP Endpoint'leri

### 2.1. Sağlık Kontrolü (`/health`)
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
*   **HTTP:** `200 OK` - Sağlıklı, `503 Service Unavailable` - Model hazır değil.

---

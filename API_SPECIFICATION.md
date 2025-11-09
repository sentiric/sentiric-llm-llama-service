# 📡 API Spesifikasyonu

## GRPC Service

### Service Definition
```protobuf
service LLMLocalService {
  rpc LocalGenerateStream(LocalGenerateStreamRequest) 
      returns (stream LocalGenerateStreamResponse);
}
```

### Message Types
```protobuf
message LocalGenerateStreamRequest {
  string prompt = 1;
  GenerationParams params = 2;
}

message LocalGenerateStreamResponse {
  string token = 1;
  FinishDetails finish_details = 2;
}

message GenerationParams {
  float temperature = 1;
  int32 top_k = 2;
  float top_p = 3;
  float repetition_penalty = 4;
  int32 max_new_tokens = 5;
}
```

### Default Parameters
```json
{
  "temperature": 0.8,
  "top_k": 40,
  "top_p": 0.95,
  "repetition_penalty": 1.1,
  "max_new_tokens": 2048
}
```

## HTTP Endpoints

### Health Check
```http
GET /health
Response: {
  "status": "healthy",
  "model_ready": true,
  "engine": "llama.cpp"
}
```

## Client Examples

### GRPC Client (C++)
```cpp
LlamaClient client(grpc::CreateChannel(
    "localhost:16061", 
    grpc::InsecureChannelCredentials()
));

// Streaming response
client.GenerateStream("Türkiye'nin başkenti neresidir?");
```

### Python Client
```python
import grpc
import sentiric_llm_v1_local_pb2 as pb
import sentiric_llm_v1_local_pb2_grpc as grpc_lib

channel = grpc.insecure_channel('localhost:16061')
stub = grpc_lib.LLMLocalServiceStub(channel)

request = pb.LocalGenerateStreamRequest(
    prompt="Türkiye'nin başkenti neresidir?"
)

for response in stub.LocalGenerateStream(request):
    print(response.token, end='', flush=True)
```

## Error Handling

### GRPC Status Codes
- `OK` (0): Başarılı
- `CANCELLED` (1): Client bağlantıyı kapattı
- `INVALID_ARGUMENT` (3): Boş prompt
- `UNAVAILABLE` (14): Model hazır değil
- `INTERNAL` (13): Sistem hatası

### HTTP Status Codes
- `200 OK`: Sağlıklı
- `503 Service Unavailable`: Model hazır değil

## Performance Characteristics

### Response Times
- **First Token**: <100ms (warm context)
- **Streaming Latency**: <10ms/token
- **Throughput**: ~50 tokens/saniye

### Resource Usage
- **Memory**: ~2.5GB sabit
- **CPU**: 4 thread optimal
- **Network**: Minimal (local only)

## Security Considerations

- **Authentication**: None (local service)
- **Encryption**: None (local network)
- **Rate Limiting**: Implement edilecek
- **Input Validation**: Basic prompt validation
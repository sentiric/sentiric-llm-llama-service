# ⚙️ Yapılandırma Rehberi

Bu servis, `docker-compose.yml` dosyası veya doğrudan sistem ortam değişkenleri aracılığıyla yapılandırılır. Tüm değişkenler `LLM_LLAMA_SERVICE_` öneki ile başlar.

## Ortam Değişkenleri Referansı

### Sistem ve Loglama
| Değişken | Açıklama | Varsayılan Değer |
|---|---|---|
| `ENV` | Çalışma ortamını belirler (`development` veya `production`). `production` modunda loglar JSON formatında yazılır. | `development` |
| `LLM_LLAMA_SERVICE_LOG_LEVEL` | Log seviyesi: `trace`, `debug`, `info`, `warn`, `error`, `critical`. | `info` |

### Network
| Değişken | Açıklama | Varsayılan Değer |
|---|---|---|
| `LLM_LLAMA_SERVICE_LISTEN_ADDRESS` | Servisin dinleyeceği IP adresi. `0.0.0.0` tüm arayüzleri dinler. | `0.0.0.0` |
| `LLM_LLAMA_SERVICE_HTTP_PORT` | HTTP health check ve metrik sunucusunun portu. | `16070` |
| `LLM_LLAMA_SERVICE_GRPC_PORT` | gRPC sunucusunun portu. | `16071` |
| `LLM_LLAMA_SERVICE_METRICS_PORT`| Prometheus metrikleri için port. | `16072`|

### Model Yönetimi
| Değişken | Açıklama | Varsayılan Değer |
|---|---|---|
| `LLM_LLAMA_SERVICE_MODEL_DIR` | Modellerin indirileceği ve saklanacağı konteyner içindeki dizin. | `/models` |
| `LLM_LLAMA_SERVICE_MODEL_ID` | Hugging Face repo ID'si (ör: `microsoft/Phi-3-mini-4k-instruct-gguf`). | `microsoft/Phi-3-mini-4k-instruct-gguf` |
| `LLM_LLAMA_SERVICE_MODEL_FILENAME` | İndirilecek GGUF dosyasının tam adı. | `Phi-3-mini-4k-instruct-q4.gguf` |

### Motor ve Performans
| Değişken | Açıklama | Varsayılan Değer |
|---|---|---|
| `LLM_LLAMA_SERVICE_GPU_LAYERS` | GPU'ya yüklenecek model katmanı sayısı. `0` sadece CPU, `-1` tüm katmanlar. | `0` |
| `LLM_LLAMA_SERVICE_CONTEXT_SIZE` | Modelin maksimum context penceresi. | `4096` |
| `LLM_LLAMA_SERVICE_THREADS` | Eş zamanlı işlenebilecek istek sayısı. Bu değer, `LlamaContextPool`'daki `llama_context` sayısını belirler. Kaynak (RAM/VRAM) kullanımını doğrudan etkiler. | `3` |
| `LLM_LLAMA_SERVICE_THREADS_BATCH`| `llama.cpp`'nin batch işleme için kullanacağı thread sayısı. | `3` |

### Varsayılan Sampling Parametreleri
*Bu değerler, sadece gRPC isteğinde özel bir parametre gönderilmediğinde kullanılır.*
| Değişken | Açıklama | Varsayılan Değer |
|---|---|---|
| `LLM_LLAMA_SERVICE_DEFAULT_MAX_TOKENS` | Varsayılan maksimum üretilecek token sayısı. | `1024` |
| `LLM_LLAMA_SERVICE_DEFAULT_TEMPERATURE` | Varsayılan sampling sıcaklığı. | `0.8` |
| `LLM_LLAMA_SERVICE_DEFAULT_TOP_K` | Varsayılan Top-K sampling değeri. | `40` |
| `LLM_LLAMA_SERVICE_DEFAULT_TOP_P` | Varsayılan Top-P (nucleus) sampling değeri. | `0.95` |
| `LLM_LLAMA_SERVICE_DEFAULT_REPEAT_PENALTY` | Varsayılan tekrar cezası. | `1.1` |

### Güvenlik (mTLS)
| Değişken | Açıklama |
|---|---|
| `GRPC_TLS_CA_PATH` | Kök sertifika (CA) dosyasının yolu. |
| `LLM_LLAMA_SERVICE_CERT_PATH` | Sunucu sertifika zinciri dosyasının yolu. |
| `LLM_LLAMA_SERVICE_KEY_PATH` | Sunucu özel anahtar dosyasının yolu. |
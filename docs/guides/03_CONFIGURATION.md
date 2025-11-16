# ⚙️ Yapılandırma Rehberi (v2.0)

Bu servis, `docker-compose.yml` dosyası veya doğrudan sistem ortam değişkenleri aracılığıyla yapılandırılır.

### Sistem ve Loglama
| Değişken | Açıklama | Varsayılan Değer |
|---|---|---|
| `ENV` | Çalışma ortamı (`development` veya `production`). `production` modunda loglar JSON formatındadır. | `development` |
| `LLM_LLAMA_SERVICE_LOG_LEVEL` | Log seviyesi: `trace`, `debug`, `info`, `warn`, `error`. | `info` |

### Network
| Değişken | Açıklama | Varsayılan Değer |
|---|---|---|
| `LLM_LLAMA_SERVICE_LISTEN_ADDRESS`| Servisin dinleyeceği IP adresi. | `0.0.0.0` |
| `LLM_LLAMA_SERVICE_HTTP_PORT` | HTTP health check sunucusunun portu. | `16070` |
| `LLM_LLAMA_SERVICE_GRPC_PORT` | gRPC sunucusunun portu. | `16071` |
| `LLM_LLAMA_SERVICE_METRICS_PORT`| Prometheus metrikleri için port. | `16072`|

### Model Yönetimi
| Değişken | Açıklama | Varsayılan Değer |
|---|---|---|
| `LLM_LLAMA_SERVICE_MODEL_DIR` | Modellerin saklanacağı dizin. | `/models` |
| `LLM_LLAMA_SERVICE_MODEL_ID` | Hugging Face repo ID'si. | `ggml-org/gemma-3-4b-it-GGUF`|
| `LLM_LLAMA_SERVICE_MODEL_FILENAME`| İndirilecek GGUF dosyasının adı. | `gemma-3-4b-it-Q4_K_M.gguf` |

### Motor ve Performans Ayarları
| Değişken | Açıklama | Varsayılan Değer |
|---|---|---|
| `LLM_LLAMA_SERVICE_GPU_LAYERS` | GPU'ya yüklenecek model katmanı sayısı. `0` sadece CPU, `-1` tüm katmanlar. | `0` |
| `LLM_LLAMA_SERVICE_CONTEXT_SIZE` | Modelin maksimum context penceresi. | `8192` |
| `LLM_LLAMA_SERVICE_THREADS` | Eş zamanlı işlenebilecek istek sayısı (`LlamaContextPool` boyutu). | `8` |
| `LLM_LLAMA_SERVICE_THREADS_BATCH`| `llama.cpp`'nin batch işleme için kullanacağı thread sayısı. | `8` |
| `LLM_LLAMA_SERVICE_USE_MMAP` | `true` ise, modelin tamamını RAM'e yüklemek yerine diskten okur. RAM'i kısıtlı sistemler için kritiktir. | `true` |
| `LLM_LLAMA_SERVICE_KV_OFFLOAD` | `true` ise, KV cache'i GPU'ya (VRAM) yükler. Büyük context'lerde üretim hızını ciddi artırır. | `true` |
| `LLM_LLAMA_SERVICE_NUMA` | Çoklu CPU soketli sunucular için NUMA stratejisi. (`disabled`, `dist`, `isolate`, `numactl`).| `disabled` |

### Varsayılan Sampling Parametreleri
*Bu değerler, gRPC isteğinde özel parametre gönderilmediğinde kullanılır.*
| Değişken | Açıklama | Varsayılan Değer |
|---|---|---|
| `LLM_LLAMA_SERVICE_DEFAULT_MAX_TOKENS` | Maksimum üretilecek token sayısı. | `1024` |
| `LLM_LLAMA_SERVICE_DEFAULT_TEMPERATURE`| Sampling sıcaklığı. | `0.8` |
| `LLM_LLAMA_SERVICE_DEFAULT_TOP_K` | Top-K sampling değeri. | `40` |
| `LLM_LLAMA_SERVICE_DEFAULT_TOP_P` | Top-P sampling değeri. | `0.95` |
| `LLM_LLAMA_SERVICE_DEFAULT_REPEAT_PENALTY`| Tekrar cezası. | `1.1` |

### Güvenlik (mTLS)
| Değişken | Açıklama |
|---|---|
| `GRPC_TLS_CA_PATH` | Kök sertifika (CA) dosyasının yolu. |
| `LLM_LLAMA_SERVICE_CERT_PATH` | Sunucu sertifika zinciri dosyasının yolu. |
| `LLM_LLAMA_SERVICE_KEY_PATH` | Sunucu özel anahtar dosyasının yolu. |
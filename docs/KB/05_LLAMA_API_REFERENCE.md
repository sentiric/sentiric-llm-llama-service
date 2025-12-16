 💡 KB-05: llama.cpp API Hızlı Referans Kılavuzu (vB7415)

**Kapsam:** `llama.h` ve `common.h` dosyalarındaki en kritik fonksiyonların, projemizdeki kullanım amaçlarına göre kategorize edilmiş listesidir.

---

### 1. Kurulum ve Yükleme (Setup)

| Fonksiyon | Kaynak | Açıklama |
| :--- | :--- | :--- |
| `llama_backend_init` | `llama.h` | Global arka ucu başlatır (NUMA vb.). |
| `llama_model_default_params` | `llama.h` | Varsayılan model parametrelerini döndürür. |
| `llama_model_load_from_file` | `llama.h` | GGUF dosyasından modeli yükler. |
| `llama_context_default_params` | `llama.h` | Varsayılan context parametrelerini döndürür. |
| **`llama_init_from_model`** | `llama.h` | Yüklü modelden bir çıkarım oturumu (context) oluşturur. |
| `llama_model_get_vocab` | `llama.h` | Modelin kelime dağarcığına (`llama_vocab`) pointer döner. |

### 2. Kelime Dağarcığı ve Tokenizasyon (Vocab)

*Not: Bu fonksiyonlar artık `llama_vocab*` parametresi alır.*

| Fonksiyon | Açıklama |
| :--- | :--- |
| **`llama_tokenize`** | Metni token ID listesine çevirir. `add_special` parametresi önemlidir. |
| **`llama_token_to_piece`** | Token ID'yi metin parçasına (string) çevirir. |
| `llama_vocab_n_tokens` | Toplam token sayısını döner. |
| `llama_vocab_is_eog` | Token'ın bitiş (End-Of-Generation/EOS/EOT) tokenı olup olmadığını döner. |
| `llama_vocab_is_control` | Kontrol tokenı olup olmadığını döner. |
| `llama_vocab_bos` / `eos` | Özel tokenların ID'lerini döner. |

### 3. İş Yığını (Batching) ve Çıkarım (Inference)

| Fonksiyon | Kaynak | Açıklama |
| :--- | :--- | :--- |
| `llama_batch_init` | `llama.h` | Verilen kapasitede boş bir batch oluşturur. |
| **`common_batch_add`** | `common.h` | Batch'e token ekleyen yardımcı fonksiyondur (pos, seq_id, logits ayarlarını yapar). |
| `common_batch_clear` | `common.h` | Batch'i sıfırlar (tekrar kullanım için). |
| **`llama_decode`** | `llama.h` | Batch'teki tokenları modele işler (Forward pass). KV Cache güncellenir. |
| `llama_get_logits_ith` | `llama.h` | Decode sonrası, belirtilen indexteki tokenın logit vektörünü döner. |
| `llama_get_embeddings` | `llama.h` | (Eğer aktifse) Embedding vektörünü döner. |

### 4. Bellek Yönetimi (KV Cache)

*Not: Tüm işlemler `llama_get_memory(ctx)` ile alınan nesne üzerinden yapılır.*

| Fonksiyon | Açıklama |
| :--- | :--- |
| **`llama_get_memory`** | Context'in bellek kontrolcüsünü (`llama_memory_t`) döner. |
| **`llama_memory_seq_rm`** | Belirtilen aralıktaki tokenları bellekten siler. |
| `llama_memory_seq_add` | Belirtilen aralıktaki tokenların pozisyonlarını kaydırır (Context shifting). |
| `llama_memory_seq_cp` | Bir sequence'i kopyalar (Beam search veya parallel decoding için). |
| `llama_memory_clear` | Tüm belleği tamamen temizler. |

### 5. Örnekleme (Sampling)

*Not: `llama_sampler` yapısı kullanılır.*

| Fonksiyon | Açıklama |
| :--- | :--- |
| `llama_sampler_chain_init` | Yeni bir örnekleme zinciri oluşturur. |
| `llama_sampler_chain_add` | Zincire bir kural (sampler) ekler. |
| `llama_sampler_init_top_k` | Top-K örnekleyici oluşturur. |
| `llama_sampler_init_top_p` | Top-P (Nucleus) örnekleyici oluşturur. |
| `llama_sampler_init_temp` | Sıcaklık (Temperature) örnekleyici oluşturur. |
| `llama_sampler_init_penalties` | Tekrar cezası (Repetition penalty) örnekleyicisi oluşturur. |
| **`llama_sampler_init_dist`** | Olasılık dağılımına göre rastgele seçim yapan nihai örnekleyici (Seed alır). |
| **`llama_sampler_init_greedy`** | En yüksek olasılıklı tokenı seçen nihai örnekleyici. |
| **`llama_sampler_sample`** | Logitleri işleyip zincir kurallarına göre bir token seçer. |
| **`llama_sampler_accept`** | Seçilen tokenı zincire bildirir (Internal state güncellemesi için). |

### 6. LoRA Adaptörleri (Adapters)

*LoRA'lar modelin ağırlıklarını değiştirmeden ince ayar (fine-tune) yeteneği ekler.*

| Fonksiyon | Kaynak | Açıklama |
| :--- | :--- | :--- |
| **`llama_adapter_lora_init`** | `llama.h` | Verilen yoldan (`.gguf`) bir LoRA adaptörü yükler ve `model` ile ilişkilendirir. Pointer döner. |
| **`llama_set_adapter_lora`** | `llama.h` | Yüklenmiş bir adaptörü, belirtilen `scale` (güç) faktörü ile `ctx` (context)'e uygular. |
| `llama_rm_adapter_lora` | `llama.h` | Belirtilen adaptörü context'ten çıkarır (etkisizleştirir). |
| `llama_clear_adapter_lora` | `llama.h` | Context üzerindeki **tüm** adaptörleri temizler. |
| `llama_adapter_lora_free` | `llama.h` | Adaptörü manuel olarak bellekten siler. (Çağrılmazsa `llama_model_free` otomatik siler). |
| `common_adapter_lora_info` | `common.h` | Adaptör path'i ve scale değerini tutan yardımcı struct. |

### 7. Kaynak Serbest Bırakma (Cleanup)

| Fonksiyon | Açıklama |
| :--- | :--- |
| `llama_batch_free` | Batch belleğini temizler. |
| `llama_sampler_free` | Sampler zincirini temizler. |
| `llama_free` | Context belleğini temizler. |
| `llama_model_free` | Model belleğini (ve bağlı LoRA'ları) temizler. |
| `llama_backend_free` | Kütüphaneyi kapatır. |
```
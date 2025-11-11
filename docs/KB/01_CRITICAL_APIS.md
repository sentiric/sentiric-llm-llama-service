# 💡 KB-01: Kritik llama.cpp API Referansları

Bu doküman, projemizde kullandığımız ve sık değişen `llama.cpp` API fonksiyonlarının güncel kullanım kalıplarını belgeler. Kod yazarken bu referansı kullanın.

---

### 1. Başlatma ve Yükleme Sırası

```cpp
// 1. Backend'i başlat (program başına bir kez)
llama_backend_init();

// 2. Model parametrelerini ayarla ve modeli yükle
llama_model_params model_params = llama_model_default_params();
llama_model* model = llama_model_load_from_file(model_path.c_str(), model_params);

// 3. Modelin vokabülerini al
const llama_vocab* vocab = llama_model_get_vocab(model);

// 4. Context parametrelerini ayarla ve context'i oluştur
llama_context_params ctx_params = llama_context_default_params();
ctx_params.n_ctx = 4096; // Ayarlanabilir
llama_context* ctx = llama_init_from_model(model, ctx_params);
```

### 2. Token Üretim (Inference) Döngüsü

```cpp
// 1. Gelen metni tokenize et
std::vector<llama_token> tokens(prompt_text.size());
int n_tokens = llama_tokenize(
    vocab, 
    prompt_text.c_str(), 
    prompt_text.size(), 
    tokens.data(), 
    tokens.size(), 
    true, // Add BOS (Beginning of Sequence) token
    false
);
tokens.resize(n_tokens);

// 2. Prompt'u işle (initial decode)
llama_batch batch = llama_batch_get_one(tokens.data(), tokens.size());
if (llama_decode(ctx, batch) != 0) {
    // Hata yönetimi
}

// 3. Yeni token'ları üretme döngüsü
int n_decoded = 0;
while (n_decoded < max_new_tokens) {
    // 3a. Bir sonraki token'ı örnekle (-1: son token'ın logit'lerinden)
    llama_token new_token_id = llama_sampler_sample(sampler, ctx, -1);

    // 3b. Örnekleyiciye seçilen token'ı bildir (repetition penalty gibi state'leri günceller)
    llama_sampler_accept(sampler, new_token_id);

    // 3c. Üretimin sonuna gelindi mi diye kontrol et
    if (llama_vocab_is_eog(vocab, new_token_id)) {
        break;
    }

    // 3d. Token'ı metne çevir ve stream et/kullan
    char piece;
    int n_chars = llama_token_to_piece(vocab, new_token_id, piece, sizeof(piece), 0, false);
    std::string token_str(piece, n_chars);
    // on_token_callback(token_str);

    // 3e. Bir sonraki decode işlemi için tek token'lık yeni bir batch hazırla
    batch = llama_batch_get_one(&new_token_id, 1);

    // 3f. Yeni token'ı işle
    if (llama_decode(ctx, batch) != 0) {
        // Hata yönetimi
    }
    n_decoded++;
}
```

### 3. Temizlik Sırası

```cpp
// Context'i serbest bırak
llama_free(ctx);

// Modeli serbest bırak
llama_model_free(model);

// Backend'i serbest bırak (program sonunda bir kez)
llama_backend_free();
```


---

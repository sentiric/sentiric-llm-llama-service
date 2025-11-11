# Kritik API Referansları: llama.cpp

## Model ve Context Yönetimi
Model `llama_model_load_from_file` ile yüklenir. Context `llama_init_from_model` ile oluşturulur.
```cpp
llama_model_params model_params = llama_model_default_params();
llama_model* model = llama_model_load_from_file(path, model_params);

llama_context_params ctx_params = llama_context_default_params();
llama_context* ctx = llama_init_from_model(model, ctx_params);
```

## Token Üretimi (Streaming)
Üretim, `llama_sampler_sample` ve döngü içinde `llama_decode` ile yapılır.
```cpp
// ... (prompt decode edildikten sonra)
while (/* ... */) {
    llama_token new_token_id = llama_sampler_sample(sampler, ctx, -1);
    llama_sampler_accept(sampler, new_token_id);
    if (llama_vocab_is_eog(vocab, new_token_id)) break;
    // ...
    llama_batch batch = llama_batch_get_one(&new_token_id, 1);
    llama_decode(ctx, batch);
}
```
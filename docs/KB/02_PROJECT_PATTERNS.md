# Proje Desenleri ve Standartları

## Eşzamanlılık Modeli: LlamaContextPool
Servis, birden çok isteği paralel işlemek için bir context havuzu kullanır. Her istek havuzdan bir `llama_context` alır, işlemi tamamlar ve context'i `llama_memory_seq_rm` ile temizleyerek havuza geri bırakır.

## Docker Build Stratejisi
Proje, `libllama.so` ve bağımlılıklarını dinamik olarak linkler. `Dockerfile`, `builder` aşamasında üretilen tüm `.so` dosyalarını `runtime` aşamasına kopyalamalı ve `ldconfig` çalıştırmalıdır. Statik linkleme yapılmamaktadır.
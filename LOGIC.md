# 🧬 Domain Logic & Çekirdek Algoritmalar

Bu belge, `sentiric-llm-llama-service`'in saf `llama.cpp`'den ayrılarak nasıl "Production-Grade" bir sunucuya dönüştüğünü sağlayan kritik algoritmaları ve C++ bellek yönetimi sırlarını açıklar.

## 1. Smart Context Caching (Akıllı Önbellek)
Her gelen RAG (Retrieval-Augmented Generation) isteğinde sistem promptunu ve uzun bağlam metnini baştan hesaplamak TTFT'yi (İlk Token Süresi) saniyelere çıkarır.
* **Algoritma:** `LlamaContextPool::acquire()` metodu, gelen isteğin tokenları ile havuzdaki boşta olan (Idle) `llama_context`'lerin son token dizilerini karşılaştırır.
* **Prefix Matching:** En uzun ortak başlangıcı (Longest Matching Prefix) bulan Context seçilir. Uyuşmayan kuyruk kısmı `llama_memory_seq_rm` ile anında silinir. Bu sayede 4000 tokenlık bir RAG isteğinin 3900 tokenı yeniden kullanılabilir (Cache Hit). TTFT süresi 15ms'ye kadar düşer.

## 2. Context Shifting (Sonsuz Metin İşleme)
Modelin `CONTEXT_SIZE` limitinden (Örn: 4096) daha büyük bir metin gelirse sistem çökmez.
* **Algoritma:** KV Cache dolduğunda, `llama_memory_seq_rm` ile en eski (başlangıç) kısımdaki tokenlar atılır ve `llama_memory_seq_add` ile kalan tokenlar geriye kaydırılır (Shift). Model "hafıza kaybı" yaşasa da çalışmaya devam eder.

## 3. LoRA LRU Cache (VRAM Koruması)
Her kullanıcının farklı bir LoRA adaptörü (Örn: Hukukçu, Sağlıkçı) kullanabileceği durumlarda GPU VRAM'i hızla tükenir.
* **Kural:** Sistem aynı anda en fazla `MAX_LORA_CACHE_SIZE = 8` adet benzersiz LoRA adaptörünü VRAM'de tutar.
* **Algoritma:** 9. adaptör yükleneceğinde, **LRU (Least Recently Used)** algoritması devreye girer. En uzun süredir kullanılmayan adaptör `llama_adapter_lora_free` ile VRAM'den atılır.

## 4. Güvenli VRAM Formülü
Sistemin çökmemesi (OOM) için GPU katmanları ve thread sayısı şu formüle tabidir:
**Toplam VRAM ≈ Model Ağırlığı + ( `THREADS` × Context Başına KV VRAM )**
* *Örnek:* 6GB VRAM'de 4 Thread (Eşzamanlı Kullanıcı) ve 4096 Context, yaklaşık 5.2GB tüketir ve güvenlidir.
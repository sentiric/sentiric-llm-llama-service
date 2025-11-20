# 📋 Sentiric LLM Llama Service - Görev ve Yol Haritası (Faz 3)

---
## 🎯 PROJE DURUMU
- **Faz 1 (Temel):** Tamamlandı ✅
- **Faz 2 (Stabilite & Performans):** Tamamlandı ✅ (v2.0 - Paralel Batching & Queue)
- **Faz 3 (Yetenek & Arayüz):** Başlıyor 🚀

---
## ⏳ AKTİF GÖREVLER (FAZ 3 - UI & Structured Output)

-   **[ ] TASK ID: `UI-REVAMP-001` - Sentiric Studio UI Modernizasyonu (DeepSeek Style)**
    *   **Açıklama:** Mevcut basit Vue.js arayüzü yerine, sol panelli, çoklu oturum destekli, profesyonel "DeepSeek" tasarım diline sahip HTML/CSS yapısının entegre edilmesi.
    *   **Özellikler:**
        *   [ ] Sol Sidebar (Oturumlar/Projeler)
        *   [ ] Genişletilebilir Sohbet Alanı
        *   [ ] Sağ Panel (Bağlam/Analiz)
        *   [ ] Mobil Uyumlu Responsive Tasarım
    *   **Motivasyon:** Geliştirici deneyimini artırmak ve motorun gücünü görselleştirmek.

-   **[ ] TASK ID: `UI-VOICE-001` - Basit Sesli Komut (Speech-to-Text)**
    *   **Açıklama:** Studio arayüzüne, tarayıcı tabanlı (Web Speech API) bir mikrofon butonu eklenmesi.
    *   **Hedef:** IP sinyal işleme senaryoları için sesli prompt girişini simüle etmek.
    *   **Not:** Backend STT servisi kullanılmayacak, frontend API yeterli.

-   **[ ] TASK ID: `LLM-GRAMMAR-001` - Structured Output (JSON Mode & GBNF)**
    *   **Açıklama:** Motorun çıktısını belirli bir şemaya (JSON Schema) zorlamak için `llama.cpp` grammar (GBNF) desteğinin `LLMEngine`'e eklenmesi.
    *   **Kullanım:** Agent ve Gateway servislerinin kararlı veri alabilmesi için kritik.
    *   **API:** İsteğe `json_schema` veya `grammar` alanı eklenecek.

---
## 🎯 PLANLANAN GÖREVLER (FAZ 4 - Entegrasyon)

-   **[ ] TASK ID: `GW-CONN-001` - LLM Gateway gRPC Entegrasyonu**
    *   **Açıklama:** Bu servisin, merkezi `llm-gateway-service` tarafından bir "worker" olarak tanınması ve yönetilmesi.

---
## ✅ TAMAMLANAN KRİTİK GÖREVLER

-   **[✓] `LLM-CORE-FIX`:** Double-Free bellek hatası `ContextGuard` manual release ile çözüldü.
-   **[✓] `LLM-HTTP-FIX`:** Stack Overflow hatası `Producer-Consumer Queue` mimarisi ile çözüldü.
-   **[✓] `LLM-PERF-002`:** Gerçek Paralel İşleme (Thread Dispatching) eklendi. (1.13x overhead ile %100 paralellik).
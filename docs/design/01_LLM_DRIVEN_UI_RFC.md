# RFC-001: LLM-Güdümlü Dinamik Arayüz Mimarisi

- **Başlık:** LLM-Güdümlü Dinamik Arayüz Mimarisi
- **Durum:** Taslak (Draft)
- **Tarih:** 2025-12-10
- **Yazar:** CTO / Baş Mimar

---

## 1. Özet (Abstract)

Bu belge, klasik, statik kullanıcı arayüzü (UI) paradigmalarından, yapay zeka tarafından dinamik olarak oluşturulan ve yönlendirilen, çok modlu (multi-modal) bir işbirliği ortamına geçiş için bir mimari teklifi sunmaktadır. Amaç, UI'ı sabit bir yapı olmaktan çıkarıp, kullanıcının amacına göre uyum sağlayan, LLM'in araçları ve bileşenleri yönetebildiği esnek bir çalışma alanına dönüştürmektir.

---

## 2. Genel Hedef (Overall Goal)

Mevcut UI anlayışını temelden değiştirerek, aşağıdaki özelliklere sahip yeni nesil bir arayüz oluşturmak:

-   **Çok Modlu (Multi-Modal):** Metin, konuşma, görsel ve API veri akışlarını tek bir tutarlı arayüzde birleştirir.
-   **Dinamik ve Üretken (Dynamic & Generative):** Kullanıcının görevine ve amacına göre UI bileşenlerini (paneller, araçlar, formlar) dinamik olarak oluşturur, düzenler ve kaldırır.
-   **LLM Tarafından Yönlendirilebilir (LLM-Orchestrated):** Arayüz, LLM'in anlayabileceği ve manipüle edebileceği modüler bileşenlerden oluşur. LLM, bu bileşenleri kullanarak kullanıcıya yardımcı olur ve iş akışlarını otomatikleştirir.

Nihai vizyon, UI'ın bir dizi butondan ibaret olmadığı, **yapay zeka ile insan arasında bir işbirliği alanı** haline geldiği bir sistemdir.

---

## 3. Temel Gereksinimler (Core Requirements)

### 3.1. Çok Modlu Girdi Desteği
Arayüz, aşağıdaki girdi türlerini sorunsuz bir şekilde kabul etmeli ve işlemelidir:
-   Metin girişi
-   Sesli komutlar
-   Görsel yükleme (sürükle-bırak)
-   Harici API'lerden gelen veri akışları
-   Kullanıcının eylemlerini doğal dilde tarif etmesi

### 3.2. Dinamik/Generatif UI Yapısı
LLM, kullanıcının amacına göre UI'ı anlık olarak şekillendirebilmelidir.
-   İlgili panelleri ve araç setlerini otomatik olarak ekleyip kaldırabilme.
-   Görevle ilgili form, kart, tablo veya grafikleri oluşturabilme.

> **Örnek Senaryo:**
> Kullanıcı "Yeni bir pazarlama kampanyası planlayalım" dediğinde, LLM otomatik olarak şu bileşenleri içeren bir çalışma alanı oluşturabilir:
> 1.  Bir **Zaman Çizelgesi (Timeline)** paneli.
> 2.  Bir **Görev Listesi (Task List)** aracı.
> 3.  İlgili dokümanlar için bir **Döküman Görüntüleyici**.
> 4.  Hızlı notlar için bir **Not Defteri** alanı.

### 3.3. Ajan ve Araç (Tool) Entegrasyonu
LLM'in UI üzerinde anlamlı eylemler gerçekleştirebilmesi için bir **"Araç Kayıt Sistemi" (Tool Registry)** olmalıdır. Her araç şu meta verileri içermelidir:
-   `name`: Aracın benzersiz adı (örn: `create_table`).
-   `description`: Aracın ne işe yaradığının doğal dil açıklaması.
-   `parameters`: Gerekli parametreler ve tipleri.
-   `ui_effect`: Aracın UI üzerindeki etkisi (örn: "yeni bir tablo oluşturur", "bir popup açar").

### 3.4. Durum (State) Yönetimi
LLM'in UI'ın mevcut durumunu anlayabilmesi için, sistem düzenli olarak LLM'e bir **"Durum Anlık Görüntüsü" (State Snapshot)** göndermelidir. Bu özet şunları içermelidir:
-   Açık olan paneller ve araçlar.
-   Kullanıcının seçtiği aktif öğeler.
-   Kullanıcının beyan ettiği son amaç (intent).
-   Gerçekleşen son birkaç işlem.

### 3.5. Geri Bildirim ve Onay Mekanizması
LLM'in önerdiği kritik UI değişiklikleri veya eylemleri için kullanıcı onayı alınmalıdır.
-   Öneri bir bildirim olarak sunulmalı: `[Uygula]`, `[Reddet]`, `[Düzenle]`.
-   Özellikle veri silme veya gönderme gibi geri döndürülemez işlemler asla tam otomatik olmamalıdır.

### 3.6. Kişiselleştirme Altyapısı
LLM, kullanıcı davranışlarını analiz ederek arayüzü kişiselleştirmelidir.
-   Sık kullanılan araçları öne çıkarma.
-   Panel yerleşimlerini kullanıcının iş akışına göre optimize etme.
-   Tercih edilen renk modu veya tema gibi ayarları önerme.

---

## 4. Yazılım Mimarisi (Software Architecture)

### 4.1. Modüler ve Tanımlanabilir UI Bileşenleri
Tüm UI bileşenleri, **JSON veya benzeri bir DSL (Domain-Specific Language)** ile tanımlanmalıdır. Bu, LLM'in bileşenleri programatik olarak okumasını, anlamasını ve yeni bileşen tanımları oluşturmasını sağlar. Mimari, "component-driven" ve "LLM-friendly" olmalıdır.

### 4.2. Çift Yönlü İletişim Protokolü
LLM ve UI arasında standart bir iletişim katmanı kurulmalıdır. Bu protokol, aşağıdaki temel eylemleri desteklemelidir:
-   `UI -> LLM`: `sendContext`, `sendEvent` (örn: kullanıcı butona tıkladı).
-   `LLM -> UI`: `getAction`, `applyAction` (örn: `create_table` aracını çalıştır).

### 4.3. Logging ve Telemetri
LLM tarafından tetiklenen tüm UI eylemleri, analiz ve hata ayıklama için detaylı olarak kaydedilmelidir:
-   Çağrılan araçlar ve parametreleri.
-   Gerçekleştirilen UI değişiklikleri (DOM mutasyonları).
-   Kullanıcının LLM önerilerine verdiği tepkiler (onay/red).

---

## 5. Tasarım İlkeleri (Design Principles)

### 5.1. LLM Uyumlu Bileşen Kütüphanesi
Tasarım sistemi (component library), her bileşenin amacını, parametrelerini ve durumlarını LLM'in anlayabileceği şekilde belgelemelidir.

### 5.2. Esnek ve Adaptif Yerleşim (Layout) Sistemi
Tasarım, sabit pikseller yerine, dinamik olarak yeniden düzenlenebilen grid yapıları, bölünebilir paneller ve genişleyebilir konteynerler üzerine kurulmalıdır.

---

## 6. Güvenlik ve Gizlilik (Security & Privacy)

-   **İzin Listeleri (Allow-lists):** LLM'in UI üzerinde gerçekleştirebileceği eylemler (örn: API çağırma, dosya silme) kesin olarak tanımlanmış bir izin listesi ile kısıtlanmalıdır.
-   **Veri Maskeleme:** Hassas veriler (API anahtarları, kişisel bilgiler), state snapshot'ları LLM'e gönderilmeden önce maskelenmelidir.
-   **Açık Onay:** Kullanıcı verilerinin işlenmesi veya üçüncü parti servislere gönderilmesi her zaman açık kullanıcı onayı gerektirmelidir.

---

## 7. Hazırlık Yol Haritası (Preparation Roadmap)

| Alan | Gereksinim | Durum |
| :--- | :--- | :--- |
| **Teknik** | Çok modlu giriş altyapısı | Planlama 📝 |
| **Teknik** | Component-driven UI mimarisi (örn: React, Svelte) | Planlama 📝 |
| **Teknik** | JSON/DSL tabanlı UI tanım şeması | Planlama 📝 |
| **Teknik** | Tool registry ve API entegrasyon katmanı | Planlama 📝 |
| **Teknik** | State snapshot ve iletişim protokolü | Planlama 📝 |
| **Teknik** | Eylem onaylama (confirm/deny) mekanizması | Planlama 📝 |
| **Tasarım** | LLM uyumlu, belgelenmiş bileşen kütüphanesi | Planlama 📝 |
| **Tasarım** | Adaptif ve modüler layout sistemi | Planlama 📝 |
| **Ürün** | Öncelikli kullanım senaryoları ve iş akışları | Planlama 📝 |
| **Güvenlik** | LLM eylem sınırları ve veri maskeleme politikası | Planlama 📝 |
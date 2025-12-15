# 🏗️ Sistem Mimarisi (v2.5)

## 1. Modüler Controller Yapısı

v2.5 ile birlikte monolitik yapı terk edilmiş ve **Separation of Concerns (SoC)** prensibine göre sorumluluklar dağıtılmıştır:

-   **`SystemController`:** Donanım ayarları, sağlık kontrolü ve statik dosya sunumu (Güvenlikli).
-   **`ModelController`:** Model indirme, profil değiştirme ve validasyon.
-   **`ChatController`:** İstek işleme, prompt formatlama ve streaming yanıt yönetimi.

## 2. Dynamic Batcher ve TTFT

Sistem, gelen istekleri anında işlemek yerine (Naive approach), milisaniyeler mertebesinde (varsayılan 5ms) bekleyerek gruplar (`DynamicBatcher`).

-   **Avantajı:** GPU'nun paralel işlem gücünden faydalanarak Throughput (TPS) artırılır.
-   **Metrik:** Her istek için **Time-To-First-Token (TTFT)** ölçülür ve loglanır. Bu, telefon görüşmesindeki "sessizlik süresini" temsil ettiği için en kritik SLA metriğidir.

## 3. Güvenlik Katmanı

-   **Path Traversal Koruması:** `SystemController` içinde `std::filesystem::canonical` kullanılarak, dosya sistemi üzerinden yetkisiz erişimler (örn: `../../.env`) engellenmiştir.
-   **Input Sanitization:** `ChatController`, gelen tokenlardaki geçersiz UTF-8 karakterlerini ve kontrol karakterlerini temizler.
```

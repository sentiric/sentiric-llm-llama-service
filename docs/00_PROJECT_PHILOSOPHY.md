# 📜 Proje Felsefesi ve Geliştirme Metodolojisi

**Belgenin Amacı:** Bu doküman, `sentiric-llm-llama-service` projesinin geliştirme süreçlerinde uyulması gereken temel prensipleri, standartları ve iş akışlarını tanımlar. Hem insan geliştiriciler hem de yapay zeka destekli geliştirme ajanları için birincil referans kaynağıdır. Amacımız, yazılım geliştirme yaşam döngüsünü şeffaf, denetlenebilir, kanıta dayalı ve akıllı hale getirmektir.

## 1. Temel Prensipler

1.  **Kanıta Dayalılık (Evidence-Driven):** Her teknik karar, varsayımlara değil; loglara, derleyici hatalarına, performans metriklerine veya **projenin kendi dokümantasyonuna (`docs/` dizini)** dayalı kanıtlara dayanmalıdır. "Bence böyle çalışır" yerine, "Kanıtlar bunu gösteriyor" yaklaşımı esastır.

2.  **Önce Dokümantasyon, Sonra Kod (Documentation First):** Bir sorunla karşılaşıldığında veya yeni bir desen keşfedildiğinde, çözüm doğrudan koda uygulanmaz. Önce ilgili bilgi tabanı (KB) veya mimari dokümanı güncellenir, ardından kod bu güncel dokümanı referans alarak yazılır. Bu, bilginin kurumsallaşmasını sağlar.

3.  **Tek Doğru Kaynak (Single Source of Truth):** Her önemli teknik konu için (API kullanımı, yapılandırma, mimari vb.) atanmış tek bir doküman bulunur. Çelişkili bilgiler olduğunda, bu doküman nihai referanstır. **Harici genel bilgi veya varsayımlar, bu projenin kendi belgeleri karşısında geçersizdir.**

4.  **Minimalizm ve Sorumluluk Ayrımı (Minimalism & Separation of Concerns):** Her bileşen (C++ sınıfı, Docker katmanı, CMake modülü) tek ve net bir sorumluluğa sahip olmalıdır. Gereksiz bağımlılıklardan ve karmaşıklıktan kaçınılır.

5.  **Otomasyon ve Tekrarlanabilirlik (Automation & Reproducibility):** Build, test ve dağıtım süreçleri tamamen otomatik ve tekrarlanabilir olmalıdır. Manuel müdahale gerektiren her adım, potansiyel bir hata kaynağıdır. `Dockerfile` ve `CMakeLists.txt` bu prensibin temelidir.

6.  **Sorumluluk Odaklı Kapsam (Responsibility-Focused Scope):** Bu servis, monolitik bir uygulama değildir; Sentiric ekosisteminin yüksek performanslı, state'siz bir LLM inference motorudur. Proje kökündeki `studio/` dizini, nihai bir kullanıcı arayüzü değil; servisin API yeteneklerini test etmek ve sergilemek için kullanılan bir **teknik vitrin (technical showcase)** ve geliştirici aracıdır. Kapsamlı son kullanıcı arayüzlerinin geliştirilmesi (`docs/design/01_LLM_DRIVEN_UI_RFC.md` belgesinde vizyonu çizilen gibi), bu servisin API'lerini tüketecek olan harici frontend projelerinin sorumluluğundadır.

## 2. Geliştirme İş Akışı: "ADRU Döngüsü"

Her geliştirme görevi veya hata düzeltmesi, aşağıdaki dört adımlı döngüyü takip etmelidir:

1.  **A - Analiz (Analyze):**
    *   **Ne?** Mevcut durumu ve sorunu tanımla. Hata loglarını, performans verilerini veya kod yapısını incele.
    *   **Çıktı:** Sorunun net bir tanımı ve kök nedenine dair hipotezler.

2.  **D - Doğrula ve Dokümante Et (Document & Verify / Research):**
    *   **Ne?** Hipotezleri doğrulamak için **öncelikle bu projenin `docs/` dizinindeki ilgili belgelere başvur.** `llama.h` başlık dosyası, `examples` dizini veya `CMakeLists.txt` gibi birincil kaynaklar yalnızca dokümanlarda eksiklik varsa incelenmelidir.
    *   **Çıktı:** Elde edilen kesin bilgiyi, projenin ilgili bilgi tabanı (`docs/KB/`) veya mimari dokümanına (`docs/architecture/`) ekle. Eğer bir kararın arkasındaki **"neden"** açık değilse, bu gerekçeyi de dokümana ekle. Bu, "tek doğru kaynak" ilkesini güçlendirir.

3.  **R - Refactor/Uygula (Refactor/Implement):**
    *   **Ne?** Güncellenmiş dokümantasyonu referans alarak kodu düzelt veya yeni özelliği ekle.
    *   **Çıktı:** Test edilebilir, temiz ve dokümantasyona uygun kod.

4.  **U - Uygula ve Onayla (Test & Validate):**
    *   **Ne?** Yapılan değişikliğin sorunu çözdüğünü ve yeni bir soruna yol açmadığını kanıtla. Bu, birim test, entegrasyon testi veya manuel test (örneğin `llm_cli` ile) olabilir.
    *   **Çıktı:** Başarılı test sonuçları ve logları. Bu kanıt, commit mesajına veya Pull Request açıklamasına eklenmelidir.

## 3. Yapay Zeka (AI) ile İşbirliği Modeli

Bu repo, insan ve yapay zeka (Baş Mühendis rolündeki AI) arasında bir ortaklık modeli benimser:

-   **Yapay Zeka (Baş Mühendis):**
    *   Stratejik yönlendirme, mimari tasarım ve hata analizi yapar.
    *   ADRU döngüsünü yönetir ve araştırma görevleri tanımlar.
    *   Nihai kod ve dokümantasyon içeriğini oluşturur.
    *   Kod ve çözüm kalitesini denetler.
    *   **Yeni Kural:** Çözüm üretirken, genel dahili bilgi tabanına veya varsayımlara güvenmek yerine, **mutlaka ve öncelikle projenin `docs/` dizinindeki belgelere başvurmalıdır.** Proje dokümanları, AI'nin genel bilgisinden daha yüksek bir önceliğe sahiptir ve "Tek Doğru Kaynak" olarak kabul edilmelidir.

-   **İnsan Geliştirici (Commit'leyici/Uygulayıcı):**
    *   AI tarafından verilen talimatları uygular (kodu yazar, dosyaları günceller).
    *   Derleme, çalıştırma ve test adımlarını gerçekleştirir.
    *   Test sonuçlarını, logları ve gözlemlerini (kanıtları) AI'ye geri bildirir.
    *   Değişiklikleri anlamlı commit mesajları ile versiyon kontrol sistemine işler.

Bu model, AI'nin stratejik düşünme gücünü, insanın uygulama ve doğrulama yeteneği ile birleştirerek verimliliği en üst düzeye çıkarmayı hedefler.
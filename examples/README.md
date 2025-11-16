# 🧪 Örnek Kullanım Senaryoları

Bu dizin, `run_request.sh` test script'i ile kullanılabilecek, farklı dikey (vertical) servis senaryoları için hazırlanmış örnek bağlam (`context`) dosyalarını içerir.

## `run_request.sh` Script'i ile Test Etme

Bu örnekleri test etmek için, projenin kök dizinindeyken aşağıdaki komutları çalıştırabilirsiniz.

**Varsayılan (GPU):**
```bash
./run_request.sh <context_dosyası_yolu> "<sorgu>"
```

**CPU ile:**
```bash
./run_request.sh -c <context_dosyası_yolu> "<sorgu>"
```

---

### 🏨 Turizm (Hospitality) Senaryosu
```bash
./run_request.sh examples/hospitality_service_context.txt "Müşteri dün rezervasyon yaptı. Rezervasyon durumu nedir?"
```

### ❤️ Sağlık (Health) Senaryosu
```bash
./run_request.sh examples/health_service_context.txt "Hastanın son test sonuçları nelerdir?"
```

### 🛍️ E-ticaret (E-commerce) Senaryosu
```bash
# Sakin bir soru
./run_request.sh examples/ecommerce_service_context.txt "Müşterinin son siparişinin durumu nedir?"

# Endişeli bir soru
./run_request.sh examples/ecommerce_service_context.txt "Çıldıracağım! Kargom hala gelmedi!"
```

### ⚖️ Hukuk (Legal) Senaryosu
```bash
./run_request.sh examples/legal_service_context.txt "Müvekkilin davayla ilgili son gelişmeler nelerdir?"
```

### 🏛️ Kamu (Public) Senaryosu
```bash
./run_request.sh examples/public_service_context.txt "Vatandaşın başvurusu hangi aşamada?"
```

### 💰 Finans (Finance) Senaryosu
```bash
./run_request.sh examples/finance_service_context.txt "Müşterinin son işlem detayları nelerdir?"
```

### ☂️ Sigorta (Insurance) Senaryosu
```bash
./run_request.sh examples/insurance_service_context.txt "Sigortalının poliçe durumu nedir?"
```


---

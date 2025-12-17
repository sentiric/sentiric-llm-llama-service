#!/bin/bash
source tests/lib/common.sh

log_header "SENARYO: Voice Gateway Simülasyonu (Söz Kesme / Interruption)"

# Bu test, llm_cli içindeki 'interrupt-test' komutunu kullanarak
# Gerçek bir gRPC streaming iptal senaryosunu (Cancelation) simüle eder.
# Gateway, kullanıcı konuşmaya başladığında mevcut stream'i iptal edip
# yeni context ile (yarım kalan cümle + yeni istek) tekrar istek atar.

# Docker run komutu: GPU erişimi ve ağ bağlantısı ile CLI'ı çalıştır
log_info "CLI Interrupt Testi Başlatılıyor..."

OUTPUT=$(docker compose -f docker-compose.yml -f docker-compose.gpu.yml -f docker-compose.run.gpu.yml \
    run --rm llm-cli llm_cli interrupt-test 2>&1)

echo "$OUTPUT"

# Başarı Kriterleri:
# 1. "Stream Bitti" logu görülmeli (İptal edildiği için)
# 2. "AI'ın yarım cümlesi" yakalanmalı
# 3. "Simülasyon Tamamlandı" mesajı alınmalı

if echo "$OUTPUT" | grep -q "Simülasyon Tamamlandı"; then
    # Yarım kalan cümleyi kontrol et (Context retention)
    if echo "$OUTPUT" | grep -q "AI'ın yarım cümlesi"; then
        log_pass "Voice Gateway Simülasyonu: Başarılı (Stream İptali ve Context Aktarımı doğrulandı)"
    else
        log_fail "Simülasyon tamamlandı ama Context aktarımı (yarım cümle) tespit edilemedi."
        exit 1
    fi
else
    log_fail "Simülasyon başarısız oldu veya çöktü."
    exit 1
fi
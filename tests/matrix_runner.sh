#!/bin/bash
source tests/lib/common.sh

PROFILES_FILE="models/profiles.json"

if [ ! -f "$PROFILES_FILE" ]; then
    log_fail "Profil dosyası bulunamadı: $PROFILES_FILE"
    exit 1
fi

# Profilleri oku
PROFILES=$(jq -r '.profiles | keys[]' "$PROFILES_FILE")

log_header "🚀 MATRİS TEST BAŞLATILIYOR"
log_info "Bulunan Profiller: $(echo $PROFILES | tr '\n' ' ')"

FAILED_PROFILES=()

# 1. Önce Ağı Kontrol Et/Oluştur
ensure_network

# 2. Servisi ayağa kaldır
# HATA KONTROLÜ EKLENDİ: Compose başarısız olursa testi durdur.
if ! docker compose -f docker-compose.yml -f docker-compose.gpu.yml -f docker-compose.gpu.override.yml up -d --remove-orphans; then
    log_fail "Docker Compose başlatılamadı! Lütfen logları kontrol edin."
    exit 1
fi

wait_for_service

for profile in $PROFILES; do
    echo "----------------------------------------------------------------"
    log_header "TEST EDİLİYOR: $profile"
    
    # Model Değiştir
    if ! switch_profile "$profile"; then
        FAILED_PROFILES+=("$profile (Load Failed)")
        continue
    fi
    
    # Test Suitlerini Çalıştır
    
    # Suit A: Telefon Simülasyonu
    if ./tests/suites/01_phone_simulation.sh; then
        log_pass "Suite 01: Phone Sim"
    else
        log_fail "Suite 01: Phone Sim"
        FAILED_PROFILES+=("$profile (Phone Sim Failed)")
        continue
    fi

    # Suit B: Teknik Kontroller
    if ./tests/suites/02_technical_checks.sh; then
         log_pass "Suite 02: Technical"
    else
         log_fail "Suite 02: Technical"
         FAILED_PROFILES+=("$profile (Technical Failed)")
         continue
    fi

    # Suit C: Mini Stress
    if ./tests/suites/03_stress_mini.sh; then
         log_pass "Suite 03: Stress"
    else
         log_fail "Suite 03: Stress"
         FAILED_PROFILES+=("$profile (Stress Failed)")
    fi
    
done

echo "----------------------------------------------------------------"
if [ ${#FAILED_PROFILES[@]} -eq 0 ]; then
    echo -e "${GREEN}🎉 TÜM PROFİLLER VE TESTLER BAŞARILI!${NC}"
    exit 0
else
    echo -e "${RED}⚠️ BAZI PROFİLLER BAŞARISIZ OLDU:${NC}"
    for fail in "${FAILED_PROFILES[@]}"; do
        echo -e "  - $fail"
    done
    exit 1
fi
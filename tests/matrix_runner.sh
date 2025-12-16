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

# Servisi ayağa kaldır (Eğer kapalıysa)
docker compose -f docker-compose.yml -f docker-compose.gpu.yml -f docker-compose.gpu.override.yml up -d
wait_for_service

for profile in $PROFILES; do
    echo "----------------------------------------------------------------"
    log_header "TEST EDİLİYOR: $profile"
    
    # 1. Modeli Değiştir
    if ! switch_profile "$profile"; then
        FAILED_PROFILES+=("$profile (Load Failed)")
        continue
    fi
    
    # 2. Test Suitlerini Çalıştır
    
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

    # Suit C: Mini Stress (Opsiyonel - Hızlı test için comment out yapılabilir)
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
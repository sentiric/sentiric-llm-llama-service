#!/bin/bash
source tests/lib/common.sh

PROFILES_FILE="models/profiles.json"
REPORT_FILE="test_report.csv"

echo "Model,Test,Status,Latency(ms),Note" > "$REPORT_FILE"

if [ ! -f "$PROFILES_FILE" ]; then
    log_fail "Profil dosyası bulunamadı: $PROFILES_FILE"
    exit 1
fi

# Profilleri oku
PROFILES=$(jq -r '.profiles | keys[]' "$PROFILES_FILE")

log_header "🚀 MATRİS TEST BAŞLATILIYOR (V2.5 ULTIMATE)"
log_info "Test Edilecek Profiller: $(echo $PROFILES | tr '\n' ' ')"

FAILED_PROFILES=()

ensure_network

docker compose -f docker-compose.yml -f docker-compose.gpu.yml -f docker-compose.gpu.override.yml down --remove-orphans > /dev/null 2>&1
docker compose -f docker-compose.yml -f docker-compose.gpu.yml -f docker-compose.gpu.override.yml up -d

chmod +x tests/suites/*.sh
wait_for_service

for profile in $PROFILES; do
    echo "----------------------------------------------------------------"
    log_header "TEST EDİLİYOR: $profile"
    
    if ! switch_profile "$profile"; then
        FAILED_PROFILES+=("$profile (Load Failed)")
        continue
    fi
    
    # 1. Phone Sim
    START=$(date +%s%N)
    if ./tests/suites/01_phone_simulation.sh; then
        END=$(date +%s%N); DUR=$(( (END - START) / 1000000 ))
        echo "$profile,PhoneSim,PASS,$DUR," >> "$REPORT_FILE"
    else
        FAILED_PROFILES+=("$profile (Phone Sim)")
    fi

    # 2. Technical
    if ./tests/suites/02_technical_checks.sh; then
         echo "$profile,Technical,PASS,0," >> "$REPORT_FILE"
    else
         FAILED_PROFILES+=("$profile (Technical)")
    fi

    # 3. Logic
    if ./tests/suites/04_long_context.sh; then
         echo "$profile,Logic,PASS,0," >> "$REPORT_FILE"
    else
         FAILED_PROFILES+=("$profile (Logic)")
    fi

    # 4. Advanced Capabilities (YENİ)
    if ./tests/suites/05_advanced_capabilities.sh; then
         echo "$profile,Advanced,PASS,0," >> "$REPORT_FILE"
         log_pass "Suite 05: Advanced Capabilities"
    else
         FAILED_PROFILES+=("$profile (Advanced)")
         log_fail "Suite 05: Advanced Capabilities"
    fi

    # 5. Stress
    OUTPUT=$(./tests/suites/03_stress_mini.sh)
    if echo "$OUTPUT" | grep -q "Stress testi tamamlandı"; then
         TPS=$(echo "$OUTPUT" | grep "Token/Saniye" | awk '{print $3}' | tr -d '\r')
         echo "$profile,Stress,PASS,0,TPS: $TPS" >> "$REPORT_FILE"
    else
         FAILED_PROFILES+=("$profile (Stress)")
    fi
done

echo "----------------------------------------------------------------"
column -t -s, "$REPORT_FILE"

if [ ${#FAILED_PROFILES[@]} -eq 0 ]; then
    echo -e "${GREEN}🎉 MÜKEMMEL! TÜM TESTLER BAŞARIYLA GEÇİLDİ.${NC}"
    exit 0
else
    echo -e "${RED}⚠️ BAZI TESTLER BAŞARISIZ OLDU:${NC}"
    exit 1
fi
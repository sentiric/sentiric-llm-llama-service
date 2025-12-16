#!/bin/bash
source tests/lib/common.sh

PROFILES_FILE="models/profiles.json"
REPORT_FILE="test_report.csv"

# Rapor Başlığı
echo "Model,Test,Status,Latency(ms),Note" > "$REPORT_FILE"

if [ ! -f "$PROFILES_FILE" ]; then
    log_fail "Profil dosyası bulunamadı: $PROFILES_FILE"
    exit 1
fi

PROFILES=$(jq -r '.profiles | keys[]' "$PROFILES_FILE")

log_header "🚀 MATRİS TEST BAŞLATILIYOR (PERFORMANS ANALİZ MODU)"
log_info "Bulunan Profiller: $(echo $PROFILES | tr '\n' ' ')"

FAILED_PROFILES=()

ensure_network

if ! docker compose -f docker-compose.yml -f docker-compose.gpu.yml -f docker-compose.gpu.override.yml up -d --remove-orphans; then
    log_fail "Docker Compose başlatılamadı!"
    exit 1
fi

wait_for_service

for profile in $PROFILES; do
    echo "----------------------------------------------------------------"
    log_header "TEST EDİLİYOR: $profile"
    
    if ! switch_profile "$profile"; then
        FAILED_PROFILES+=("$profile (Load Failed)")
        echo "$profile,Load,FAIL,0,Model Load Failed" >> "$REPORT_FILE"
        continue
    fi
    
    # --- TEST 1: Phone Sim ---
    START=$(date +%s%N)
    if ./tests/suites/01_phone_simulation.sh; then
        END=$(date +%s%N)
        DUR=$(( (END - START) / 1000000 ))
        log_pass "Suite 01: Phone Sim (${DUR}ms)"
        echo "$profile,PhoneSim,PASS,$DUR," >> "$REPORT_FILE"
    else
        log_fail "Suite 01: Phone Sim"
        FAILED_PROFILES+=("$profile (Phone Sim Failed)")
        echo "$profile,PhoneSim,FAIL,0,Content Mismatch" >> "$REPORT_FILE"
    fi

    # --- TEST 2: Technical ---
    START=$(date +%s%N)
    if ./tests/suites/02_technical_checks.sh; then
         END=$(date +%s%N)
         DUR=$(( (END - START) / 1000000 ))
         log_pass "Suite 02: Technical (${DUR}ms)"
         echo "$profile,Technical,PASS,$DUR," >> "$REPORT_FILE"
    else
         log_fail "Suite 02: Technical"
         FAILED_PROFILES+=("$profile (Technical Failed)")
         echo "$profile,Technical,FAIL,0,JSON/SystemPrompt Fail" >> "$REPORT_FILE"
    fi

    # --- TEST 3: Stress ---
    # Stress testi çıktısını parse et
    OUTPUT=$(./tests/suites/03_stress_mini.sh)
    echo "$OUTPUT"
    
    if echo "$OUTPUT" | grep -q "Stress testi tamamlandı"; then
         TPS=$(echo "$OUTPUT" | grep "TPS" | awk '{print $NF}')
         log_pass "Suite 03: Stress (TPS: $TPS)"
         echo "$profile,Stress,PASS,0,TPS: $TPS" >> "$REPORT_FILE"
    else
         log_fail "Suite 03: Stress"
         FAILED_PROFILES+=("$profile (Stress Failed)")
         echo "$profile,Stress,FAIL,0,Crash" >> "$REPORT_FILE"
    fi
    
done

echo "----------------------------------------------------------------"
echo "📊 TEST RAPORU OLUŞTURULDU: $REPORT_FILE"
column -t -s, "$REPORT_FILE"

if [ ${#FAILED_PROFILES[@]} -eq 0 ]; then
    echo -e "${GREEN}🎉 TÜM PROFİLLER VE TESTLER BAŞARILI!${NC}"
    exit 0
else
    echo -e "${RED}⚠️ BAZI PROFİLLER BAŞARISIZ OLDU:${NC}"
    exit 1
fi
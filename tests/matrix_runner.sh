#!/bin/bash
source tests/lib/common.sh

PROFILES_FILE="models/profiles.json"
REPORT_FILE="test_report.csv"
LOG_FILE="test_run.log"

> "$LOG_FILE"
exec > >(tee -a "$LOG_FILE") 2>&1

echo "Model,Test,Status,Latency(ms),Note" > "$REPORT_FILE"

if [ ! -f "$PROFILES_FILE" ]; then
    log_fail "Profil dosyası bulunamadı: $PROFILES_FILE"
fi

PROFILES=$(jq -r '.profiles | keys[]' "$PROFILES_FILE")

log_header "🚀 SENTIRIC VERTICALS VALIDATION MATRIX (V2.8.0 - WARM BOOT)"
log_info "Log Dosyası: $LOG_FILE"

ensure_network
log_info "Ortam sıfırlanıyor..."
docker compose -f docker-compose.yml -f docker-compose.gpu.yml -f docker-compose.gpu.override.yml down --remove-orphans > /dev/null 2>&1
docker compose -f docker-compose.yml -f docker-compose.gpu.yml -f docker-compose.gpu.override.yml up -d

chmod +x tests/suites/*.sh
wait_for_service

for profile in $PROFILES; do
    echo "----------------------------------------------------------------"
    log_header "TEST EDİLİYOR: $profile"
    
    # switch_profile artık kendi içinde prime_model çağırıyor.
    if ! switch_profile "$profile"; then
        continue
    fi
    
    # B. Test 1: Telefon Simülasyonu (Isınmış Engine ile)
    START=$(date +%s%N)
    if ./tests/suites/01_phone_simulation.sh; then
        END=$(date +%s%N); DUR=$(( (END - START) / 1000000 ))
        log_pass "Suite 01: Phone Sim (${DUR}ms)"
        echo "$profile,PhoneSim,PASS,$DUR," >> "$REPORT_FILE"
    else
        log_fail "Suite 01: Phone Sim"
    fi

    # ... (Geri kalan testler aynı kalsın) ...
    
    # C. Test 2: Teknik Kontroller
    START=$(date +%s%N)
    if ./tests/suites/02_technical_checks.sh; then
         END=$(date +%s%N); DUR=$(( (END - START) / 1000000 ))
         log_pass "Suite 02: Technical (${DUR}ms)"
         echo "$profile,Technical,PASS,$DUR," >> "$REPORT_FILE"
    else
         log_fail "Suite 02: Technical"
    fi

    # D. Test 3: Uzun Bağlam ve Mantık (Logic)
    START=$(date +%s%N)
    if ./tests/suites/04_long_context.sh; then
         END=$(date +%s%N); DUR=$(( (END - START) / 1000000 ))
         log_pass "Suite 04: Long Context & Logic (${DUR}ms)"
         echo "$profile,Logic,PASS,$DUR," >> "$REPORT_FILE"
    else
         log_fail "Suite 04: Long Context & Logic"
    fi

    # E. Test 4: İleri Düzey Yetenekler
    START=$(date +%s%N)
    if ./tests/suites/05_advanced_capabilities.sh; then
         END=$(date +%s%N); DUR=$(( (END - START) / 1000000 ))
         log_pass "Suite 05: Advanced Capabilities (${DUR}ms)"
         echo "$profile,Advanced,PASS,$DUR," >> "$REPORT_FILE"
    else
         log_fail "Suite 05: Advanced Capabilities"
    fi

    # F. Sektörel Dikey Akışlar
    for flow_script in tests/suites/*_flow.sh; do
        if [[ "$flow_script" =~ (01_|02_|03_|04_|05_|14_) ]]; then continue; fi
        test_name=$(basename "$flow_script" .sh)
        START=$(date +%s%N)
        if ./$flow_script; then
             END=$(date +%s%N); DUR=$(( (END - START) / 1000000 ))
             log_pass "Suite $(echo $test_name | cut -d'_' -f1): $test_name (${DUR}ms)"
             echo "$profile,Vertical_$test_name,PASS,$DUR," >> "$REPORT_FILE"
        else
             log_fail "Suite $(echo $test_name | cut -d'_' -f1): $test_name"
        fi
    done

    # G. Test 14: Voice Gateway Simülasyonu
    START=$(date +%s%N)
    if ./tests/suites/14_voice_gateway_simulation.sh; then
         END=$(date +%s%N); DUR=$(( (END - START) / 1000000 ))
         log_pass "Suite 14: Voice Gateway Sim (${DUR}ms)"
         echo "$profile,Voice_Sim,PASS,$DUR," >> "$REPORT_FILE"
    else
         log_fail "Suite 14: Voice Gateway Sim"
    fi
done

log_header "📊 NİHAİ TEST RAPORU"
column -t -s, "$REPORT_FILE"
echo -e "${GREEN}🎉 TEBRİKLER! TÜM SİSTEM ISINMIŞ VE DOĞRULANMIŞ DURUMDA.${NC}"
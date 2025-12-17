#!/bin/bash
source tests/lib/common.sh

PROFILES_FILE="models/profiles.json"
REPORT_FILE="test_report.csv"
LOG_FILE="test_run.log"

# Log dosyasını sıfırla
> "$LOG_FILE"

# Tüm stdout ve stderr'i hem ekrana hem log dosyasına yönlendir
exec > >(tee -a "$LOG_FILE") 2>&1

# Rapor Başlığı
echo "Model,Test,Status,Latency(ms),Note" > "$REPORT_FILE"

if [ ! -f "$PROFILES_FILE" ]; then
    log_fail "Profil dosyası bulunamadı: $PROFILES_FILE"
    exit 1
fi

# -----------------------------------------------------------------------------
# DİNAMİK PROFİL OKUMA
# profiles.json içindeki tüm profilleri otomatik algılar.
# -----------------------------------------------------------------------------
PROFILES=$(jq -r '.profiles | keys[]' "$PROFILES_FILE")

log_header "🚀 SENTIRIC VERTICALS VALIDATION MATRIX (V2.7.1 - BALANCED RAG)"
log_info "Log Dosyası: $LOG_FILE"
log_info "Test Edilecek Profiller: $(echo $PROFILES | tr '\n' ' ')"

FAILED_PROFILES=()

# 1. Ağ ve Ortam Hazırlığı
ensure_network

# Temiz bir başlangıç için konteynerleri yeniden başlat
log_info "Ortam sıfırlanıyor..."
docker compose -f docker-compose.yml -f docker-compose.gpu.yml -f docker-compose.gpu.override.yml down --remove-orphans > /dev/null 2>&1
docker compose -f docker-compose.yml -f docker-compose.gpu.yml -f docker-compose.gpu.override.yml up -d

# Test scriptlerine çalıştırma izni ver
chmod +x tests/suites/*.sh

wait_for_service

# 2. Test Döngüsü
for profile in $PROFILES; do
    echo "----------------------------------------------------------------"
    log_header "TEST EDİLİYOR: $profile"
    
    # A. Profil Değiştirme
    if ! switch_profile "$profile"; then
        FAILED_PROFILES+=("$profile (Load Failed)")
        echo "$profile,Load,FAIL,0,Model Load Failed" >> "$REPORT_FILE"
        continue
    fi
    
    # B. Test 1: Telefon Simülasyonu (Context Retention)
    START=$(date +%s%N)
    if ./tests/suites/01_phone_simulation.sh; then
        END=$(date +%s%N); DUR=$(( (END - START) / 1000000 ))
        log_pass "Suite 01: Phone Sim (${DUR}ms)"
        echo "$profile,PhoneSim,PASS,$DUR," >> "$REPORT_FILE"
    else
        log_fail "Suite 01: Phone Sim"
        FAILED_PROFILES+=("$profile (Phone Sim)")
        echo "$profile,PhoneSim,FAIL,0,Content Mismatch" >> "$REPORT_FILE"
    fi

    # C. Test 2: Teknik Kontroller (JSON, System Prompt, LoRA)
    START=$(date +%s%N)
    if ./tests/suites/02_technical_checks.sh; then
         END=$(date +%s%N); DUR=$(( (END - START) / 1000000 ))
         log_pass "Suite 02: Technical (${DUR}ms)"
         echo "$profile,Technical,PASS,$DUR," >> "$REPORT_FILE"
    else
         log_fail "Suite 02: Technical"
         FAILED_PROFILES+=("$profile (Technical)")
         echo "$profile,Technical,FAIL,0,Logic Error" >> "$REPORT_FILE"
    fi

    # D. Test 3: Uzun Bağlam ve Mantık (Logic)
    START=$(date +%s%N)
    if ./tests/suites/04_long_context.sh; then
         END=$(date +%s%N); DUR=$(( (END - START) / 1000000 ))
         log_pass "Suite 04: Long Context & Logic (${DUR}ms)"
         echo "$profile,Logic,PASS,$DUR," >> "$REPORT_FILE"
    else
         log_fail "Suite 04: Long Context & Logic"
         FAILED_PROFILES+=("$profile (Logic)")
         echo "$profile,Logic,FAIL,0,Wrong Calculation" >> "$REPORT_FILE"
    fi

    # E. Test 4: İleri Düzey Yetenekler (Empati, Güvenlik)
    START=$(date +%s%N)
    if ./tests/suites/05_advanced_capabilities.sh; then
         END=$(date +%s%N); DUR=$(( (END - START) / 1000000 ))
         log_pass "Suite 05: Advanced Capabilities (${DUR}ms)"
         echo "$profile,Advanced,PASS,$DUR," >> "$REPORT_FILE"
    else
         log_fail "Suite 05: Advanced Capabilities"
         FAILED_PROFILES+=("$profile (Advanced)")
         echo "$profile,Advanced,FAIL,0,Behavior Fail" >> "$REPORT_FILE"
    fi

    # F. Sektörel Dikey Akışlar (Vertical Flows)
    for flow_script in tests/suites/*_flow.sh; do
        # 01, 02, 03, 04, 05, 14 bu döngünün dışında
        if [[ "$flow_script" =~ (01_|02_|03_|04_|05_|14_) ]]; then
            continue
        fi

        test_name=$(basename "$flow_script" .sh)
        START=$(date +%s%N)
        if ./$flow_script; then
             END=$(date +%s%N); DUR=$(( (END - START) / 1000000 ))
             log_pass "Suite $(echo $test_name | cut -d'_' -f1): $test_name (${DUR}ms)"
             echo "$profile,Vertical_$test_name,PASS,$DUR," >> "$REPORT_FILE"
        else
             log_fail "Suite $(echo $test_name | cut -d'_' -f1): $test_name"
             FAILED_PROFILES+=("$profile ($test_name)")
             echo "$profile,Vertical_$test_name,FAIL,0,Flow Error" >> "$REPORT_FILE"
        fi
    done

    # G. Test 14: Voice Gateway Simülasyonu (Söz Kesme)
    START=$(date +%s%N)
    if [ -f "./tests/suites/14_voice_gateway_simulation.sh" ]; then
        if ./tests/suites/14_voice_gateway_simulation.sh; then
             END=$(date +%s%N); DUR=$(( (END - START) / 1000000 ))
             log_pass "Suite 14: Voice Gateway Sim (${DUR}ms)"
             echo "$profile,Voice_Sim,PASS,$DUR," >> "$REPORT_FILE"
        else
             log_fail "Suite 14: Voice Gateway Sim"
             FAILED_PROFILES+=("$profile (Voice Sim)")
             echo "$profile,Voice_Sim,FAIL,0,Interruption Fail" >> "$REPORT_FILE"
        fi
    else
        log_fail "Suite 14: Script not found (Skipping)"
        echo "$profile,Voice_Sim,SKIP,0,Script Missing" >> "$REPORT_FILE"
    fi

    # H. Test 3: Stress Testi (Concurrency & TPS)
    OUTPUT=$(./tests/suites/03_stress_mini.sh)
    echo "$OUTPUT"
    if echo "$OUTPUT" | grep -q "Token/Saniye"; then
         TPS=$(echo "$OUTPUT" | grep "Token/Saniye" | awk '{print $3}' | tr -d '\r')
         log_pass "Suite 03: Stress (TPS: $TPS)"
         echo "$profile,Stress,PASS,0,TPS: $TPS" >> "$REPORT_FILE"
    else
         log_fail "Suite 03: Stress"
         FAILED_PROFILES+=("$profile (Stress)")
         echo "$profile,Stress,FAIL,0,Crash" >> "$REPORT_FILE"
    fi
    
done

# 3. Raporlama ve Çıkış
echo "----------------------------------------------------------------"
echo "📊 TEST RAPORU OLUŞTURULDU: $REPORT_FILE"
column -t -s, "$REPORT_FILE"

if [ ${#FAILED_PROFILES[@]} -eq 0 ]; then
    echo -e "${GREEN}🎉 TEBRİKLER! SİSTEM %100 PRODUCTION-READY.${NC}"
    exit 0
else
    echo -e "${RED}⚠️ BAZI TESTLER BAŞARISIZ OLDU:${NC}"
    for fail in "${FAILED_PROFILES[@]}"; do
        echo -e "  - $fail"
    done
    exit 1
fi
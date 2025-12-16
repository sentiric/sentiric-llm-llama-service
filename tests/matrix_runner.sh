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

# -----------------------------------------------------------------------------
# DİNAMİK PROFİL OKUMA
# profiles.json içindeki tüm profilleri otomatik algılar.
# Yeni model eklendiğinde bu scripti değiştirmeye gerek yoktur.
# -----------------------------------------------------------------------------
PROFILES=$(jq -r '.profiles | keys[]' "$PROFILES_FILE")

log_header "🚀 MATRİS TEST BAŞLATILIYOR (DİNAMİK MOD)"
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
        END=$(date +%s%N)
        DUR=$(( (END - START) / 1000000 ))
        log_pass "Suite 01: Phone Sim (${DUR}ms)"
        echo "$profile,PhoneSim,PASS,$DUR," >> "$REPORT_FILE"
    else
        log_fail "Suite 01: Phone Sim"
        FAILED_PROFILES+=("$profile (Phone Sim Failed)")
        echo "$profile,PhoneSim,FAIL,0,Content Mismatch" >> "$REPORT_FILE"
    fi

    # C. Test 2: Teknik Kontroller (JSON, System Prompt, LoRA)
    START=$(date +%s%N)
    if ./tests/suites/02_technical_checks.sh; then
         END=$(date +%s%N)
         DUR=$(( (END - START) / 1000000 ))
         log_pass "Suite 02: Technical (${DUR}ms)"
         echo "$profile,Technical,PASS,$DUR," >> "$REPORT_FILE"
    else
         log_fail "Suite 02: Technical"
         FAILED_PROFILES+=("$profile (Technical Failed)")
         echo "$profile,Technical,FAIL,0,Logic Error" >> "$REPORT_FILE"
    fi

    # D. Test 3: Uzun Bağlam ve Mantık (Logic)
    START=$(date +%s%N)
    if ./tests/suites/04_long_context.sh; then
         END=$(date +%s%N)
         DUR=$(( (END - START) / 1000000 ))
         log_pass "Suite 04: Long Context & Logic (${DUR}ms)"
         echo "$profile,Logic,PASS,$DUR," >> "$REPORT_FILE"
    else
         log_fail "Suite 04: Long Context & Logic"
         FAILED_PROFILES+=("$profile (Logic Failed)")
         echo "$profile,Logic,FAIL,0,Wrong Calculation" >> "$REPORT_FILE"
    fi

    # E. Test 4: Stress Testi (Concurrency)
    OUTPUT=$(./tests/suites/03_stress_mini.sh)
    echo "$OUTPUT"
    
    if echo "$OUTPUT" | grep -q "Stress testi tamamlandı"; then
         TPS=$(echo "$OUTPUT" | grep "Token/Saniye" | awk '{print $3}' | tr -d '\r')
         log_pass "Suite 03: Stress (TPS: $TPS)"
         echo "$profile,Stress,PASS,0,TPS: $TPS" >> "$REPORT_FILE"
    else
         log_fail "Suite 03: Stress"
         FAILED_PROFILES+=("$profile (Stress Failed)")
         echo "$profile,Stress,FAIL,0,Crash" >> "$REPORT_FILE"
    fi
    
done

# 3. Raporlama ve Çıkış
echo "----------------------------------------------------------------"
echo "📊 TEST RAPORU OLUŞTURULDU: $REPORT_FILE"
column -t -s, "$REPORT_FILE"

if [ ${#FAILED_PROFILES[@]} -eq 0 ]; then
    echo -e "${GREEN}🎉 TEBRİKLER! TÜM PROFİLLER TESTLERİ GEÇTİ.${NC}"
    exit 0
else
    echo -e "${RED}⚠️ BAZI PROFİLLER BAŞARISIZ OLDU:${NC}"
    for fail in "${FAILED_PROFILES[@]}"; do
        echo -e "  - $fail"
    done
    exit 1
fi
#!/bin/bash
source tests/lib/common.sh

PROFILES_FILE="models/profiles.json"
REPORT_FILE="test_report.csv"

echo "Model,Test,Status,Latency(ms),Note" > "$REPORT_FILE"

if [ ! -f "$PROFILES_FILE" ]; then
    log_fail "Profil dosyası bulunamadı: $PROFILES_FILE"
    exit 1
fi

PROFILES=$(jq -r '.profiles | keys[]' "$PROFILES_FILE")

log_header "🚀 SENTIRIC VERTICALS VALIDATION MATRIX"
log_info "Test Edilecek Profiller: $(echo $PROFILES | tr '\n' ' ')"

ensure_network
docker compose -f docker-compose.yml -f docker-compose.gpu.yml -f docker-compose.gpu.override.yml up -d
chmod +x tests/suites/*.sh
wait_for_service

for profile in $PROFILES; do
    echo "----------------------------------------------------------------"
    log_header "TEST EDİLİYOR: $profile"
    
    if ! switch_profile "$profile"; then continue; fi
    
    # 1. TEMEL YETENEKLER
    # -------------------
    # Phone Sim (Context Retention)
    START=$(date +%s%N); ./tests/suites/01_phone_simulation.sh
    if [ $? -eq 0 ]; then
        END=$(date +%s%N); DUR=$(( (END - START) / 1000000 ))
        echo "$profile,Basic_PhoneSim,PASS,$DUR," >> "$REPORT_FILE"
    else
        echo "$profile,Basic_PhoneSim,FAIL,0," >> "$REPORT_FILE"
    fi

    # Technical Checks
    if ./tests/suites/02_technical_checks.sh; then
         echo "$profile,Basic_Technical,PASS,0," >> "$REPORT_FILE"
    fi

    # Logic & Long Context
    if ./tests/suites/04_long_context.sh; then
         echo "$profile,Basic_Logic,PASS,0," >> "$REPORT_FILE"
    fi

    # Advanced Behavior
    if ./tests/suites/05_advanced_capabilities.sh; then
         echo "$profile,Basic_Advanced,PASS,0," >> "$REPORT_FILE"
    fi

    # 2. SEKTÖREL DİKEYLER (VERTICAL FLOWS)
    # -------------------------------------
    # Tüm _flow.sh ile biten dosyaları bul ve çalıştır
    for flow_script in tests/suites/*_flow.sh; do
        test_name=$(basename "$flow_script" .sh)
        # 06_complex_flow ve 07_health_flow hariç (onlar zaten var, ama hepsini döngüye alabiliriz)
        
        START=$(date +%s%N)
        if ./$flow_script; then
             END=$(date +%s%N); DUR=$(( (END - START) / 1000000 ))
             echo "$profile,Vertical_$test_name,PASS,$DUR," >> "$REPORT_FILE"
        else
             echo "$profile,Vertical_$test_name,FAIL,0," >> "$REPORT_FILE"
        fi
    done

    # 3. PERFORMANS
    # -------------
    OUTPUT=$(./tests/suites/03_stress_mini.sh)
    if echo "$OUTPUT" | grep -q "Stress testi tamamlandı"; then
         TPS=$(echo "$OUTPUT" | grep "Token/Saniye" | awk '{print $3}' | tr -d '\r')
         echo "$profile,Perf_Stress,PASS,0,TPS: $TPS" >> "$REPORT_FILE"
    fi
done

echo "----------------------------------------------------------------"
column -t -s, "$REPORT_FILE"
#!/bin/bash
source tests/lib/common.sh

log_header "SENARYO: Mini Stress Test (Concurrency)"

# llm_cli üzerinden benchmark
# 4 Concurrency, 5 Requests = 20 İstek
log_info "4 Eşzamanlı bağlantı ile yük testi başlatılıyor..."

# [FIX] Dosya çıktısı yerine STDOUT kullanılıyor.
# Konteyner (--rm) kapandığında /tmp içindeki dosya kaybolduğu için test başarısız oluyordu.
OUTPUT=$(docker compose -f docker-compose.yml -f docker-compose.gpu.yml -f docker-compose.run.gpu.yml \
    run --rm llm-cli llm_cli benchmark --concurrent 4 --requests 5 --output "")

echo "$OUTPUT"

if echo "$OUTPUT" | grep -q "Token/Saniye"; then
    TPS=$(echo "$OUTPUT" | grep "Token/Saniye" | awk '{print $3}')
    log_pass "Stress testi tamamlandı. TPS: $TPS"
else
    log_fail "Stress testi çöktü veya rapor alınamadı."
fi
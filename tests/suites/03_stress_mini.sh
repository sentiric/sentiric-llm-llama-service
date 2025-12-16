#!/bin/bash
source tests/lib/common.sh

log_header "SENARYO: Mini Stress Test (Concurrency)"

# llm_cli üzerinden benchmark
# 4 Concurrency, 5 Requests = 20 İstek
log_info "4 Eşzamanlı bağlantı ile yük testi başlatılıyor..."

docker compose -f docker-compose.yml -f docker-compose.gpu.yml -f docker-compose.run.gpu.yml \
    run --rm llm-cli llm_cli benchmark --concurrent 4 --requests 5 --output /tmp/benchmark_res.txt

if [ $? -eq 0 ]; then
    log_pass "Stress testi tamamlandı."
    cat /tmp/benchmark_res.txt | grep "Token/Saniye"
else
    log_fail "Stress testi çöktü veya hata aldı."
fi
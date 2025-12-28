#!/bin/bash
source tests/lib/common.sh

log_header "SENARYO: Mini Stress Test (Concurrency)"

# llm_cli üzerinden benchmark
# 4 Concurrency, 5 Requests = 20 İstek
log_info "4 Eşzamanlı bağlantı ile yük testi başlatılıyor..."

# [FIX] Konteyner kapandığında silinen dosyayı okumaya çalışmak yerine
# STDOUT çıktısını doğrudan bir değişkene atıyoruz ve oradan parse ediyoruz.
# --output parametresi boş geçildiğinde Benchmark sınıfı zaten STDOUT'a yazar.

OUTPUT=$(docker compose -f docker-compose.yml run --rm llm-cli llm_cli benchmark --concurrent 4 --requests 5 --output "")

# Çıktıyı ekrana da basalım ki loglarda görelim
echo "$OUTPUT"

if echo "$OUTPUT" | grep -q "Token/Saniye"; then
    TPS=$(echo "$OUTPUT" | grep "Token/Saniye" | awk '{print $3}')
    log_pass "Stress testi tamamlandı. TPS: $TPS"
else
    log_fail "Stress testi çöktü veya rapor alınamadı."
fi
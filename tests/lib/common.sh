#!/bin/bash

# Renkler
GREEN='\033[0;32m'
RED='\033[0;31m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

API_URL="http://localhost:16070"

log_header() { echo -e "\n${CYAN}>>> $1${NC}"; }
log_info() { echo -e "${BLUE}ℹ️  $1${NC}"; }
log_pass() { echo -e "${GREEN}✅ PASS: $1${NC}"; }
# log_fail fonksiyonunu, çağıran scripti hatayla sonlandıracak şekilde güncelliyoruz.
log_fail() { 
    echo -e "${RED}❌ FAIL: $1${NC}"
    # Hata durumunda hemen çıkış yap (Exit 1)
    exit 1 
}

# AĞ KONTROLÜ VE OLUŞTURMA (DÜZELTİLDİ)
ensure_network() {
    local net_name="sentiric.cloud"
    # .env dosyasındaki değerlerle uyumlu olmalı
    local subnet="10.88.0.0/16"
    local gateway="10.88.0.1"

    if ! docker network ls --format '{{.Name}}' | grep -q "^${net_name}$"; then
        log_info "Ağ bulunamadı, oluşturuluyor: ${net_name} (Subnet: ${subnet})"
        
        docker network create \
          --driver bridge \
          --subnet ${subnet} \
          --gateway ${gateway} \
          ${net_name} || {
            log_fail "Ağ oluşturulamadı!"
            exit 1
        }
    else
        log_info "Ağ mevcut: ${net_name}"
    fi
}

# Servis Sağlık Kontrolü
wait_for_service() {
    local max_retries=60
    local count=0
    log_info "Servis ve Model Hazırlığı Bekleniyor..."
    
    while [ $count -lt $max_retries ]; do
        status=$(curl -s -m 2 "$API_URL/health" | jq -r '.model_ready' 2>/dev/null)
        if [ "$status" == "true" ]; then
            return 0
        fi
        echo -n "."
        sleep 2
        ((count++))
    done
    echo ""
    log_fail "Timeout: Servis ayağa kalkamadı."
    exit 1
}

# Profil Değiştirme
switch_profile() {
    local profile=$1
    log_info "Model Değiştiriliyor: $profile"
    
    res=$(curl -s -X POST "$API_URL/v1/models/switch" \
        -H "Content-Type: application/json" \
        -d "{\"profile\": \"$profile\"}")
        
    if echo "$res" | jq -e '.status == "success"' >/dev/null; then
        wait_for_service
        log_pass "Profil Aktif: $profile"
    else
        log_fail "Model değişimi başarısız: $res"
        exit 1
    fi
}

# Chat İsteği (Helper)
send_chat() {
    local payload=$1
    curl -s -X POST "$API_URL/v1/chat/completions" \
        -H "Content-Type: application/json" \
        -d "$payload"
}
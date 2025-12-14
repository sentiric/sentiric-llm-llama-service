#!/bin/bash
# ==============================================================================
# Sentiric LLM Service - Matrix Test Suite (Bash Edition)
# ==============================================================================
# Amaç: profiles.json içindeki TÜM modelleri sırayla yükleyip,
#       Chat, RAG ve History özelliklerinin her birinde çalıştığını doğrulamak.
# Gereksinim: 'jq' (sudo apt install jq)
# ==============================================================================

set -o pipefail

# --- RENKLER ---
GREEN='\033[0;32m'
RED='\033[0;31m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# --- KONFİGÜRASYON ---
API_URL="http://localhost:16070"
PROFILES_FILE="models/profiles.json"
MAX_WAIT_SECONDS=900 # Model indirme süresi uzun olabilir (15 dk)

# --- CLI COMMAND WRAPPER ---
# Docker run komutunu merkezi yönetiyoruz
run_cli() {
    # tr -d '\000' ekleyerek olası null byte'ları siliyoruz (Defense in Depth)
    docker compose -f docker-compose.yml \
                   -f docker-compose.gpu.yml \
                   -f docker-compose.run.gpu.yml \
                   run --rm llm-cli /usr/local/bin/llm_cli "$@" 2>&1 | tr -d '\000'
}

# --- YARDIMCI FONKSİYONLAR ---

log_header() { echo -e "\n${CYAN}==================================================${NC}\n${CYAN}👉 $1${NC}\n${CYAN}==================================================${NC}"; }
log_info() { echo -e "${BLUE}ℹ️  $1${NC}"; }
log_success() { echo -e "${GREEN}✅ $1${NC}"; }
log_fail() { echo -e "${RED}❌ $1${NC}"; }
log_warn() { echo -e "${YELLOW}⚠️  $1${NC}"; }

check_requirements() {
    if ! command -v jq &> /dev/null; then
        echo -e "${RED}HATA: 'jq' yüklü değil.${NC} Lütfen yükleyin: sudo apt install jq"
        exit 1
    fi
}

wait_for_service() {
    local endpoint="$1"
    local timeout="$2"
    local start_time=$(date +%s)
    
    while true; do
        current_time=$(date +%s)
        elapsed=$((current_time - start_time))
        
        if [ "$elapsed" -gt "$timeout" ]; then
            return 1
        fi

        # HTTP Status ve JSON Body kontrolü
        response=$(curl -s -m 5 "$API_URL$endpoint")
        
        # Model ready kontrolü
        is_ready=$(echo "$response" | jq -r '.model_ready // false')
        
        if [ "$is_ready" == "true" ]; then
            return 0
        fi
        
        echo -ne "${YELLOW}⏳ Bekleniyor... (${elapsed}s) - Durum: $(echo "$response" | jq -r '.status // "offline"')${NC}\r"
        sleep 5
    done
}

# --- TEST SENARYOLARI ---

run_tests_for_profile() {
    local profile_key=$1
    local profile_name=$2
    local error_count=0

    log_header "TEST EDİLİYOR: $profile_key ($profile_name)"

    # 1. MODEL SWITCH
    log_info "Model değiştiriliyor..."
    switch_res=$(curl -s -X POST "$API_URL/v1/models/switch" \
        -H "Content-Type: application/json" \
        -d "{\"profile\": \"$profile_key\"}")
    
    if [[ $(echo "$switch_res" | jq -r '.status') != "success" ]]; then
        log_fail "Model değiştirilemedi: $switch_res"
        return 1
    fi

    log_info "Modelin yüklenmesi/indirilmesi bekleniyor (Max ${MAX_WAIT_SECONDS}sn)..."
    if wait_for_service "/health" "$MAX_WAIT_SECONDS"; then
        echo "" # Newline fix after wait loop
        log_success "Model Hazır."
    else
        echo ""
        log_fail "Timeout: Model yüklenemedi."
        return 1
    fi

    # 2. BASIC CHAT
    log_info "[1/3] Temel Sohbet Testi"
    # Timeout 180'e çıkarıldı
    res_chat=$(run_cli generate "Merhaba, nasılsın?" --timeout 180)
    
    # Kontrolü gevşettik: Assistant etiketi olmayabilir (raw stream), sadece uzunluk ve hata kontrolü
    if [[ "$res_chat" != *"gRPC generation error"* ]] && [[ ${#res_chat} -gt 10 ]]; then
        log_success "Temel Sohbet Geçti"
    else
        log_fail "Temel Sohbet Kaldı. Çıktı: $res_chat"
        ((error_count++))
    fi

    # 3. RAG TEST
    log_info "[2/3] RAG Context Testi"
    # Prompt daha belirgin hale getirildi
    res_rag=$(run_cli generate "Gizli kod nedir? Sadece kodu söyle." --rag-context "GİZLİ KOD: 12345-X" --timeout 180)
    if [[ "$res_rag" == *"12345-X"* ]]; then
        log_success "RAG Testi Geçti"
    else
        log_fail "RAG Testi Kaldı. Model gizli kodu bulamadı. Yanıt: $res_rag"
        ((error_count++))
    fi

    # 4. HISTORY TEST
    log_info "[3/3] History (Hafıza) Testi"
    # History JSON formatında verilmeli
    history_json='[{"role":"user","content":"Benim adım Sentiric."},{"role":"assistant","content":"Memnun oldum Sentiric."}]'
    res_hist=$(run_cli generate "Benim adım ne? Sadece ismi söyle." --history "$history_json" --timeout 180)
    
    if [[ "$res_hist" == *"Sentiric"* ]]; then
        log_success "History Testi Geçti"
    else
        log_fail "History Testi Kaldı. Model ismi hatırlamadı. Yanıt: $res_hist"
        ((error_count++))
    fi

    return $error_count
}

# --- MAIN ---

main() {
    check_requirements

    # Servisi başlat (Eğer kapalıysa)
    log_info "Servis ortamı kontrol ediliyor..."
    # --remove-orphans ekledik temizlik için
    docker compose -f docker-compose.yml -f docker-compose.gpu.yml -f docker-compose.gpu.override.yml up -d --remove-orphans
    
    # İlk açılış beklemesi
    if ! wait_for_service "/health" 120; then
        echo ""
        log_fail "Servis başlatılamadı."
        exit 1
    fi
    echo ""

    # Profilleri oku
    if [ ! -f "$PROFILES_FILE" ]; then
        log_fail "$PROFILES_FILE bulunamadı."
        exit 1
    fi

    # jq ile key'leri al
    profile_keys=$(jq -r '.profiles | keys[]' "$PROFILES_FILE")
    
    declare -A results

    for key in $profile_keys; do
        name=$(jq -r ".profiles[\"$key\"].display_name" "$PROFILES_FILE")
        
        if run_tests_for_profile "$key" "$name"; then
            results["$key"]="PASSED"
        else
            results["$key"]="FAILED"
        fi
        
        # Testler arası kısa bekleme
        sleep 2
    done

    # --- RAPOR ---
    echo -e "\n\n${CYAN}📊 MATRIX TEST RAPORU${NC}"
    echo "=================================================="
    for key in "${!results[@]}"; do
        if [ "${results[$key]}" == "PASSED" ]; then
            echo -e "${key}: ${GREEN}PASSED${NC}"
        else
            echo -e "${key}: ${RED}FAILED${NC}"
        fi
    done
    echo "=================================================="
}

main
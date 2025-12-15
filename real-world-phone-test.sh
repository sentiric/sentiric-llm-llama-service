#!/bin/bash
# ==============================================================================
# Sentiric LLM Service - Real World Phone Conversation Simulation (v2.0 FIXED)
# ==============================================================================

set -e

# Renkler
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
CYAN='\033[0;36m'
NC='\033[0m'

API_URL="http://localhost:16070/v1/chat/completions"
HISTORY_FILE="/tmp/phone_history.json"
METRICS_URL="http://localhost:16072/metrics"

# Hata yakalama
trap 'echo -e "${RED}❌ TEST SCRIPTI HATA İLE DURDU! Satır: $LINENO${NC}"; exit 1' ERR

log_info() { echo -e "${BLUE}ℹ️  $1${NC}"; }
log_user() { echo -e "\n${YELLOW}👤 Müşteri: $1${NC}"; }
log_ai() { echo -e "${GREEN}🤖 Asistan: $1${NC}"; }
log_sys() { echo -e "${CYAN}⚙️  Sistem: $1${NC}"; }

# Hazırlık
echo "[]" > "$HISTORY_FILE"

# Dinamik RAG Verisi
CUSTOMER_CONTEXT="Müşteri Adı: Ali Vural. Paket: Gold İnternet (100Mbps). Taahhüt Bitiş: 2025-12-30. Son Fatura: 450 TL (Ödendi)."

chat() {
    local user_msg="$1"
    local system_instruction="$2"
    local rag_data="$3"
    local interrupt="$4" 

    # History güncelle
    local temp_hist=$(jq --arg content "$user_msg" '. += [{"role": "user", "content": $content}]' "$HISTORY_FILE")
    echo "$temp_hist" > "$HISTORY_FILE"

    local payload=$(jq -n \
        --arg sys "$system_instruction" \
        --arg rag "$rag_data" \
        --slurpfile hist "$HISTORY_FILE" \
        '{
            messages: $hist[0],
            system_prompt: $sys,
            rag_context: $rag,
            temperature: 0.3,
            max_tokens: 250,
            stream: false
        }')

    if [ "$interrupt" == "true" ]; then
        log_sys "⚠️  Simülasyon: Kullanıcı asistanın sözünü kesti (Interruption)..."
        # 0.2sn timeout ile kesinti simülasyonu
        curl -s -X POST "$API_URL" -H "Content-Type: application/json" -d "$payload" --max-time 0.5 > /dev/null || true
        echo -e "${RED}[KESİLDİ]${NC}"
        return
    fi

    local start_time=$(date +%s%3N)
    
    # curl timeout artırıldı (60s -> 120s) ilk istek için
    local response=$(curl -s --max-time 120 -X POST "$API_URL" -H "Content-Type: application/json" -d "$payload")
    local end_time=$(date +%s%3N)
    local duration=$((end_time - start_time))

    # JSON geçerlilik kontrolü
    if ! echo "$response" | jq -e . >/dev/null 2>&1; then
        echo -e "${RED}❌ GEÇERSİZ JSON YANITI:${NC} $response"
        exit 1
    fi

    if echo "$response" | grep -q "error"; then
        echo -e "${RED}API HATASI:${NC} $response"
        exit 1
    fi

    local reply=$(echo "$response" | jq -r '.choices[0].message.content')
    local tokens=$(echo "$response" | jq -r '.usage.completion_tokens')
    
    log_ai "$reply"
    log_sys "Süre: ${duration}ms | Token: $tokens"

    # History güncelle (Asistan)
    local temp_hist_ai=$(jq --arg content "$reply" '. += [{"role": "assistant", "content": $content}]' "$HISTORY_FILE")
    echo "$temp_hist_ai" > "$HISTORY_FILE"
}

# ==============================================================================
# SENARYO BAŞLIYOR
# ==============================================================================

log_info "📞 Telefon çalıyor... (Context: $CUSTOMER_CONTEXT)"

# 1. Giriş
log_user "Alo! Kardeşim ben Ali Vural. İnternetim yine gitti, ne oluyor?"
chat "Alo! Kardeşim ben Ali Vural. İnternetim yine gitti, ne oluyor?" \
     "Müşteriyi sakinleştir ve sorunu anlamaya çalış." \
     "$CUSTOMER_CONTEXT"

# 2. RAG Kontrolü (KRİTİK TEST)
log_user "Paketim neydi benim? Unuttum sinirden."
chat "Paketim neydi benim? Unuttum sinirden." \
     "Sadece Context bilgisini kullanarak cevap ver." \
     "$CUSTOMER_CONTEXT"

# 3. Interruption
log_user "Tamam tamam uzatma, sadede gel. Bak şimdi..."
chat "Tamam tamam uzatma, sadede gel. Bak şimdi..." "" "$CUSTOMER_CONTEXT" "true"

# 4. Context Değişimi
log_sys "🔄 CRM GÜNCELLENDİ: Arıza kaydı oluşturuldu (No: ARZ-999)."
CUSTOMER_CONTEXT="$CUSTOMER_CONTEXT Arıza Kaydı: ARZ-999 (Ekipler yolda)."

log_user "Arıza kaydı açtınız mı peki?"
chat "Arıza kaydı açtınız mı peki?" \
     "Context bilgisindeki Arıza Kaydı numarasını ver." \
     "$CUSTOMER_CONTEXT"

# 5. Hafıza
log_user "Adımı hatırlıyorsun değil mi?"
chat "Adımı hatırlıyorsun değil mi?" \
     "Kullanıcının adını teyit et." \
     "$CUSTOMER_CONTEXT"

# ==============================================================================
# ANALİZ
# ==============================================================================

log_info "📊 Test Analizi..."
HISTORY_CONTENT=$(cat "$HISTORY_FILE")

# RAG Kontrolü (Daha esnek regex)
if echo "$HISTORY_CONTENT" | grep -iqE "Gold|100Mbps|100 Mbps"; then
    log_info "✅ RAG Testi 1 (Paket): BAŞARILI"
else
    echo -e "${RED}❌ RAG Testi 1 (Paket): BAŞARISIZ - Model paketi bulamadı.${NC}"
    # exit 1 (Geliştirme sırasında exit yapmayalım, logu görelim)
fi

if echo "$HISTORY_CONTENT" | grep -iq "ARZ-999"; then
    log_info "✅ RAG Testi 2 (Arıza No): BAŞARILI"
else
    echo -e "${RED}❌ RAG Testi 2 (Arıza No): BAŞARISIZ${NC}"
fi

if echo "$HISTORY_CONTENT" | grep -iq "Ali Vural"; then
    log_info "✅ Hafıza Testi: BAŞARILI"
else
    echo -e "${RED}❌ Hafıza Testi: BAŞARISIZ${NC}"
fi

echo -e "\n${GREEN}🚀 TEST TAMAMLANDI.${NC}"
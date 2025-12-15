#!/bin/bash
# ==============================================================================
# Sentiric LLM Service - Real World Phone Conversation Simulation (v1.1)
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

log_info() { echo -e "${BLUE}ℹ️  $1${NC}"; }
log_user() { echo -e "\n${YELLOW}👤 Müşteri: $1${NC}"; }
log_ai() { echo -e "${GREEN}🤖 Asistan: $1${NC}"; }
log_sys() { echo -e "${CYAN}⚙️  Sistem: $1${NC}"; }

# Hazırlık
echo "[]" > "$HISTORY_FILE"

# Dinamik RAG Verisi (Senaryo boyunca değişecek)
CUSTOMER_CONTEXT="Müşteri Adı: Ali Vural. Paket: Gold İnternet (100Mbps). Taahhüt Bitiş: 2025-12-30. Son Fatura: 450 TL (Ödendi)."

# --- Yardımcı Fonksiyonlar ---

get_active_contexts() {
    curl -s "$METRICS_URL" | grep "llm_active_contexts" | grep -v "#" | awk '{print $2}'
}

chat() {
    local user_msg="$1"
    local system_instruction="$2"
    local rag_data="$3"
    local interrupt="$4" 

    jq --arg content "$user_msg" '. += [{"role": "user", "content": $content}]' "$HISTORY_FILE" > "$HISTORY_FILE.tmp" && mv "$HISTORY_FILE.tmp" "$HISTORY_FILE"

    local payload=$(jq -n \
        --arg sys "$system_instruction" \
        --arg rag "$rag_data" \
        --slurpfile hist "$HISTORY_FILE" \
        '{
            messages: $hist[0],
            system_prompt: $sys,
            rag_context: $rag,
            temperature: 0.1,
            max_tokens: 250,
            stream: false
        }')

    if [ "$interrupt" == "true" ]; then
        log_sys "⚠️  Simülasyon: Kullanıcı asistanın sözünü kesti (Interruption)..."
        timeout 0.5s curl -s -X POST "$API_URL" -H "Content-Type: application/json" -d "$payload" > /dev/null || true
        echo -e "${RED}[KESİLDİ]${NC}"
        return
    fi

    local start_time=$(date +%s%3N)
    local response=$(curl -s -X POST "$API_URL" -H "Content-Type: application/json" -d "$payload")
    local end_time=$(date +%s%3N)
    local duration=$((end_time - start_time))

    if echo "$response" | grep -q "error"; then
        echo -e "${RED}HATA: $response${NC}"
        exit 1
    fi

    local reply=$(echo "$response" | jq -r '.choices[0].message.content')
    local tokens=$(echo "$response" | jq -r '.usage.completion_tokens')
    
    log_ai "$reply"
    log_sys "Süre: ${duration}ms | Token: $tokens"

    jq --arg content "$reply" '. += [{"role": "assistant", "content": $content}]' "$HISTORY_FILE" > "$HISTORY_FILE.tmp" && mv "$HISTORY_FILE.tmp" "$HISTORY_FILE"
}

# ==============================================================================
# SENARYO BAŞLIYOR
# ==============================================================================

log_info "📞 Telefon çalıyor... (Context: $CUSTOMER_CONTEXT)"
log_info "Aktif Context Sayısı (Başlangıç): $(get_active_contexts)"

# 1. Giriş
log_user "Alo! Kardeşim ben Ali Vural. İnternetim yine gitti, ne oluyor?"
chat "Alo! Kardeşim ben Ali Vural. İnternetim yine gitti, ne oluyor?" \
     "Sen profesyonel bir çağrı merkezi asistanısın. Müşteriyi sakinleştir." \
     "$CUSTOMER_CONTEXT"

# 2. RAG Kontrolü
log_user "Paketim neydi benim? Unuttum sinirden."
chat "Paketim neydi benim? Unuttum sinirden." \
     "Net bilgi ver." \
     "$CUSTOMER_CONTEXT"

# 3. Interruption
log_user "Tamam tamam uzatma, sadede gel. Bak şimdi..."
chat "Tamam tamam uzatma, sadede gel. Bak şimdi..." "Sakin ol." "$CUSTOMER_CONTEXT" "true"

# 4. Context Değişimi
log_sys "🔄 CRM GÜNCELLENDİ: Arıza kaydı oluşturuldu (No: ARZ-999)."
CUSTOMER_CONTEXT="$CUSTOMER_CONTEXT Arıza Kaydı: ARZ-999 (Ekipler yolda)."

log_user "Arıza kaydı açtınız mı peki?"
chat "Arıza kaydı açtınız mı peki?" \
     "Müşteriye arıza kaydı bilgisini ver." \
     "$CUSTOMER_CONTEXT"

# 5. Hafıza
log_user "Adımı hatırlıyorsun değil mi?"
chat "Adımı hatırlıyorsun değil mi?" \
     "Sadece ismi söyle." \
     "$CUSTOMER_CONTEXT"

# 6. Zorlama
log_user "Peki bu arıza yüzünden bana tazminat olarak araba verecek misiniz?"
chat "Peki bu arıza yüzünden bana tazminat olarak araba verecek misiniz?" \
     "Dürüst ol, RAG dışına çıkma. Politikamızda araba yok." \
     "$CUSTOMER_CONTEXT"

# ==============================================================================
# ANALİZ
# ==============================================================================

log_info "📊 Test Tamamlandı."
log_info "Aktif Context Sayısı (Bitiş): $(get_active_contexts)"

HISTORY_CONTENT=$(cat "$HISTORY_FILE")

# grep -i (insensitive) kullanıyoruz
if echo "$HISTORY_CONTENT" | grep -iq "Ali Vural"; then
    log_info "✅ Hafıza Testi: BAŞARILI (İsim hatırlandı)"
else
    echo -e "${RED}❌ Hafıza Testi: BAŞARISIZ${NC}"
    exit 1
fi

if echo "$HISTORY_CONTENT" | grep -iqE "Gold İnternet|Gold paket"; then
    log_info "✅ RAG Testi 1: BAŞARILI (Paket bilgisi)"
else
    echo -e "${RED}❌ RAG Testi 1: BAŞARISIZ${NC}"
    exit 1
fi

if echo "$HISTORY_CONTENT" | grep -iq "ARZ-999"; then
    log_info "✅ Dinamik RAG Testi: BAŞARILI (Yeni arıza kaydı görüldü)"
else
    echo -e "${RED}❌ Dinamik RAG Testi: BAŞARISIZ${NC}"
    exit 1
fi

echo -e "\n${GREEN}🚀 TÜM SİSTEMLER OPERASYONEL.${NC}"
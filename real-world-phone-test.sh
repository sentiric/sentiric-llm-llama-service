#!/bin/bash
set -e

# Renkler
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

API_URL="http://localhost:16070/v1/chat/completions"
HISTORY_FILE="/tmp/phone_history.json"

log_ai() { echo -e "${GREEN}🤖 Asistan: $1${NC}"; }
log_user() { echo -e "\n${YELLOW}👤 Müşteri: $1${NC}"; }

# Hazırlık
echo "[]" > "$HISTORY_FILE"

# CRM'den gelen dinamik veri (Knowledge Base + CRM)
# Burada "Context Injection" yapıyoruz. Modelin bilmesi gereken TEK gerçek bu.
CUSTOMER_CONTEXT="Müşteri Kimliği: Ali Vural. 
Mevcut Paket: Gold İnternet (100Mbps Fiber).
Fatura Durumu: Ödenmiş, borç yok.
Bölgesel Durum: İstanbul/Kadıköy bölgesinde genel bir fiber altyapı çalışması var. Tahmini bitiş saati: 18:00.
Müşteri Duygu Durumu (STT'den): Gergin, hızlı konuşuyor."

chat() {
    local user_msg="$1"
    local system_instruction="$2" # Özel instruction (örn: sakinleştir)
    local rag_data="$3"

    # History güncelle
    temp_hist=$(jq --arg content "$user_msg" '. += [{"role": "user", "content": $content}]' "$HISTORY_FILE")
    echo "$temp_hist" > "$HISTORY_FILE"

    # Request Payload
    payload=$(jq -n \
        --arg sys "$system_instruction" \
        --arg rag "$rag_data" \
        --slurpfile hist "$HISTORY_FILE" \
        '{
            messages: $hist[0],
            system_prompt: $sys, 
            rag_context: $rag,
            temperature: 0.6, 
            max_tokens: 300
        }')

    # İstek Gönder
    response=$(curl -s -X POST "$API_URL" -H "Content-Type: application/json" -d "$payload")
    reply=$(echo "$response" | jq -r '.choices[0].message.content')
    
    log_ai "$reply"

    # History güncelle (Asistan)
    temp_hist_ai=$(jq --arg content "$reply" '. += [{"role": "assistant", "content": $content}]' "$HISTORY_FILE")
    echo "$temp_hist_ai" > "$HISTORY_FILE"
}

echo -e "${CYAN}--- ÇAĞRI BAŞLADI (CRM + KB ENTEGRASYONU) ---${NC}"

# 1. Sahne: Müşteri şikayetle geliyor
log_user "Alo! Kardeşim ben Ali Vural. İnternetim yine gitti, ne oluyor ya?"
chat "Alo! Kardeşim ben Ali Vural. İnternetim yine gitti, ne oluyor ya?" \
     "Müşteri gergin. Onu sakinleştir ve adıyla hitap et. Sorunu anladığını belirt." \
     "$CUSTOMER_CONTEXT"

# 2. Sahne: RAG Kontrolü (Paket ve Altyapı bilgisi)
log_user "Paketim neydi benim? Niye kesilip duruyor?"
chat "Paketim neydi benim? Niye kesilip duruyor?" \
     "CRM bilgisindeki paket adını ve bölgesel çalışma bilgisini ver." \
     "$CUSTOMER_CONTEXT"

# 3. Sahne: Israr ve Çözüm
log_user "Ne zaman gelecek peki? İşlerim var benim."
chat "Ne zaman gelecek peki? İşlerim var benim." \
     "Bölgesel çalışma notundaki saati söyle." \
     "$CUSTOMER_CONTEXT"
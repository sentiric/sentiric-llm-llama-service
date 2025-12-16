#!/bin/bash
set -e

# Renkler
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

API_URL="http://localhost:16070/v1/chat/completions"
HISTORY_FILE="/tmp/phone_history.json"

# Dosyayı sıfırla
echo "[]" > "$HISTORY_FILE"

# CRM VERİSİ
CUSTOMER_CONTEXT="Müşteri: Ali Vural. 
Paket: Gold İnternet (100Mbps).
Durum: Bölgesel arıza var, saat 20:00'de düzelecek.
Fatura: Ödenmiş."

chat() {
    local user_msg="$1"
    
    # 1. History'ye User Mesajını Ekle
    # Temp dosyası kullanarak race condition'ı önle
    jq --arg content "$user_msg" '. += [{"role": "user", "content": $content}]' "$HISTORY_FILE" > "$HISTORY_FILE.tmp" && mv "$HISTORY_FILE.tmp" "$HISTORY_FILE"

    # 2. Payload Oluştur
    payload=$(jq -n \
        --arg rag "$CUSTOMER_CONTEXT" \
        --slurpfile hist "$HISTORY_FILE" \
        '{
            messages: $hist[0],
            rag_context: $rag,
            temperature: 0.3, 
            max_tokens: 150
        }')

    # 3. İstek Gönder
    echo -e "\n${YELLOW}👤 Müşteri: $user_msg${NC}"
    response=$(curl -s -X POST "$API_URL" -H "Content-Type: application/json" -d "$payload")
    
    # Cevabı al
    reply=$(echo "$response" | jq -r '.choices[0].message.content')
    
    if [ "$reply" == "null" ]; then
        echo "HATA: Cevap alınamadı. Ham yanıt: $response"
        exit 1
    fi

    echo -e "${GREEN}🤖 Asistan: $reply${NC}"

    # 4. History'ye Asistan Mesajını Ekle
    jq --arg content "$reply" '. += [{"role": "assistant", "content": $content}]' "$HISTORY_FILE" > "$HISTORY_FILE.tmp" && mv "$HISTORY_FILE.tmp" "$HISTORY_FILE"
}

echo "--- KESİN SONUÇ TESTİ ---"

chat "Alo, Ali Vural ben. İnternetim yok!"
chat "Hangi paketi kullanıyorum ben? Unuttum."
chat "Ne zaman düzelir peki?"
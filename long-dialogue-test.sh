#!/bin/bash
# ==============================================================================
# Sentiric LLM Service - Long Context & Memory Test v2.2 (DEBUGGED)
# ==============================================================================

set -e

# Renkler
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

API_URL="http://localhost:16070/v1/chat/completions"
HEALTH_URL="http://localhost:16070/health"
HISTORY_FILE="/tmp/llm_history.json"

trap 'echo -e "\n${RED}❌ HATA: Script $LINENO satırında durdu.${NC}"; exit 1' ERR

echo "👉 Hazırlıklar Kontrol Ediliyor..."

if ! command -v jq &> /dev/null; then
    echo "jq yükleniyor..."
    apt-get update && apt-get install -y jq
fi

echo "👉 Servisin Hazır Olması Bekleniyor..."
MAX_RETRIES=40
COUNT=0
READY=false

while [ $COUNT -lt $MAX_RETRIES ]; do
    if STATUS=$(curl -s -m 5 "$HEALTH_URL"); then
        # jq exit code kontrolü için -e kullanıyoruz
        if echo "$STATUS" | jq -e '.status == "healthy" and .model_ready == true' > /dev/null; then
            READY=true
            echo -e "\n${GREEN}✅ Servis ve Model Hazır!${NC}"
            break
        fi
    fi
    echo -n "."
    sleep 3
    ((COUNT++))
done

if [ "$READY" = false ]; then
    echo -e "\n${RED}❌ Timeout: Servis hazır olamadı.${NC}"
    exit 1
fi

# --- DÖNGÜ BAŞLIYOR ---
echo "[]" > "$HISTORY_FILE"

declare -a USER_MESSAGES=(
    "Merhaba, ben Deniz."
    "Ankara'da hava nasıl?"
    "Adım neydi?" 
)

echo "👉 Diyalog Başlatılıyor..."

for MSG in "${USER_MESSAGES[@]}"; do
    echo -e "\n👤 User: $MSG"
    
    # Geçmişi güncelle
    temp_hist=$(jq --arg content "$MSG" '. += [{"role": "user", "content": $content}]' "$HISTORY_FILE")
    echo "$temp_hist" > "$HISTORY_FILE"

    # Payload
    PAYLOAD=$(jq -n \
        --slurpfile hist "$HISTORY_FILE" \
        '{
            messages: $hist[0],
            temperature: 0.3,
            max_tokens: 100
        }')

    # İstek gönder (Hata durumunda output'u göster)
    RESPONSE=$(curl -s -f -X POST "$API_URL" -H "Content-Type: application/json" -d "$PAYLOAD" || echo "CURL_ERROR")
    
    if [ "$RESPONSE" == "CURL_ERROR" ]; then
        echo -e "${RED}❌ API İsteği Başarısız!${NC}"
        exit 1
    fi

    ANSWER=$(echo "$RESPONSE" | jq -r '.choices[0].message.content')
    echo -e "${GREEN}🤖 AI: $ANSWER${NC}"

    # Asistan cevabını geçmişe ekle
    temp_hist_ai=$(jq --arg content "$ANSWER" '. += [{"role": "assistant", "content": $content}]' "$HISTORY_FILE")
    echo "$temp_hist_ai" > "$HISTORY_FILE"
    
    sleep 1
done

echo -e "\n✅ Test başarıyla tamamlandı."
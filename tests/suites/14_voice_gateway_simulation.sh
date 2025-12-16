#!/bin/bash
source tests/lib/common.sh

log_header "SENARYO: Voice Gateway Simülasyonu (V4 - Coproc ile %100 Kararlı)"

# --- Test Ayarları ---
RAG_DATA="Sipariş No: #ABC-123. Durum: Kargoya verildi. Kargo Takip No: TRK-987654321. Ürün: Bluetooth Kulaklık. Teslimat Adresi: İstanbul. Tahmini Teslim: Yarın."
DEFAULT_SYSTEM_PROMPT="Sen bir sipariş takip asistanısın. Kısa ve net bilgi ver."
HISTORY_FILE="/tmp/gateway_sim_history.json"

# --- Simülasyon Fonksiyonları ---
log_user() { echo -e "\n\033[1;34m🙍‍♂️ KULLANICI:\033[0m $1"; }
log_gateway() { echo -e "\n\033[1;33m⚡ GATEWAY:\033[0m $1"; }

# Tek bir konuşma turunu yöneten ana fonksiyon
chat_turn() {
    local user_input="$1"
    local system_prompt_override="${2:-$DEFAULT_SYSTEM_PROMPT}"

    jq --arg c "$user_input" '. += [{"role": "user", "content": $content}]' "$HISTORY_FILE" > "${HISTORY_FILE}.tmp" && mv "${HISTORY_FILE}.tmp" "$HISTORY_FILE"

    local PAYLOAD
    PAYLOAD=$(jq -n --arg rag "$RAG_DATA" --arg sys "$system_prompt_override" --slurpfile hist "$HISTORY_FILE" \
        '{ "messages": ([{"role":"system","content":$sys}] + $hist[0]), "rag_context": $rag, "stream": true, "temperature": 0.0 }')

    echo -n -e "\033[1;32m🤖 ASİSTAN:\033[0m "
    local full_response=""
    
    # AI Cevabını stream et ve yakala
    while IFS= read -r token; do
        if [ -n "$token" ]; then
            echo -n -e "\033[0;32m$token\033[0m"
            full_response+="$token"
        fi
    done < <(curl -s -N -X POST "$API_URL/v1/chat/completions" -H "Content-Type: application/json" -d "$PAYLOAD" | 
             while IFS= read -r line; do
                 if [[ $line == "data: "* && $line != *"DONE"* ]]; then
                     echo "${line:6}" | jq -r '.choices[0].delta.content // empty'
                 fi
             done)
    echo ""

    jq --arg c "$full_response" '. += [{"role": "assistant", "content": $c}]' "$HISTORY_FILE" > "${HISTORY_FILE}.tmp" && mv "${HISTORY_FILE}.tmp" "$HISTORY_FILE"
}

# --- TEST AKIŞI ---

# 1. BAŞLANGIÇ
echo "[]" > "$HISTORY_FILE"

# 2. İLK SORU
log_user "Merhaba, siparişim ne alemde? Kargoya verildi mi, kargo takip numarasını ve teslimat adresini öğrenebilir miyim?"
jq --arg c "Merhaba, siparişim ne alemde? Kargoya verildi mi, kargo takip numarasını ve teslimat adresini öğrenebilir miyim?" '. += [{"role": "user", "content": $c}]' "$HISTORY_FILE" > "${HISTORY_FILE}.tmp" && mv "${HISTORY_FILE}.tmp" "$HISTORY_FILE"

PAYLOAD=$(jq -n --arg rag "$RAG_DATA" --arg sys "$DEFAULT_SYSTEM_PROMPT" --slurpfile hist "$HISTORY_FILE" \
    '{ "messages": $hist[0], "rag_context": $rag, "stream": true, "temperature": 0.0 }')

log_gateway "LLM'e istek gönderiliyor..."
echo -n -e "\033[1;32m🤖 ASİSTAN:\033[0m "

# [FİNAL DÜZELTME] Coproc ile güvenli arka plan işlemi
coproc LLM_STREAM {
    curl -s -N -X POST "$API_URL/v1/chat/completions" -d "$PAYLOAD" -H "Content-Type: application/json" | 
    while IFS= read -r line; do
        if [[ $line == "data: "* && $line != *"DONE"* ]]; then
            echo "${line:6}" | jq -r '.choices[0].delta.content // empty'
        fi
    done
}

# AI'ın çıktısını oku ve ekrana yaz, bir yandan da değişkene kaydet
PARTIAL_RESPONSE=""
while IFS= read -r -t 1.5 token; do # 1.5 saniye bekle
    if [ -n "$token" ]; then
        echo -n -e "\033[0;32m$token\033[0m"
        PARTIAL_RESPONSE+="$token"
    fi
done <&"${LLM_STREAM[0]}"

# 3. SÖZ KESME
log_gateway "!!! VAD: KULLANICI KONUŞMAYA BAŞLADI !!!"
kill $LLM_STREAM_PID; wait $LLM_STREAM_PID 2>/dev/null
jq --arg c "$PARTIAL_RESPONSE" '. += [{"role": "assistant", "content": $c}]' "$HISTORY_FILE" > "${HISTORY_FILE}.tmp" && mv "${HISTORY_FILE}.tmp" "$HISTORY_FILE"
log_gateway "-> LLM stream kesildi. AI'ın yarım cümlesi hafızaya kaydedildi: \"$PARTIAL_RESPONSE...\""

# 4. YENİ SORU
chat_turn "Pardon, bir şey daha soracağım, teslimat adresi doğru mu, İstanbul muydu?"

# 5. FİNAL SORU (Özel Prompt ile Hafıza Testi)
MEMORY_PROMPT="Sen bir analizcisin. Sana verilen konuşma geçmişini incele ve kullanıcının en baştaki ilk sorusunu bulup tekrar et."
chat_turn "Harika. Peki en başta ne sormuştum, unuttum da." "$MEMORY_PROMPT"

log_header "Simülasyon Tamamlandı."
rm "$HISTORY_FILE"
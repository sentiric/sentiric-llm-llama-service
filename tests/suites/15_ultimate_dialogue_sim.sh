#!/bin/bash
source tests/lib/common.sh

log_header "SENARYO: Nihai Diyalog Simülasyonu (Hafıza, Söz Kesme, Konu Değişimi)"

# --- Test Ayarları ---
# İki farklı konuyu tek RAG verisinde birleştiriyoruz
RAG_DATA="SİPARİŞ BİLGİSİ: No: #ABC-123, Ürün: Bluetooth Kulaklık, Durum: Kargoya verildi, Kargo Takip: TRK-987654321. KİŞİSEL AJANDA: Bugün 15:00 Dişçi Randevusu. Yarın 09:00 Proje Toplantısı."
DEFAULT_SYSTEM_PROMPT="Sen hem sipariş takibi yapabilen hem de kişisel ajandayı yönetebilen çok yetenekli bir asistansın. Kısa ve net cevaplar ver."
HISTORY_FILE="/tmp/ultimate_sim_history.json"

# --- Simülasyon Fonksiyonları ---
log_user() { echo -e "\n\033[1;34m🙍‍♂️ KULLANICI:\033[0m $1"; }
log_gateway() { echo -e "\n\033[1;33m⚡ GATEWAY:\033[0m $1"; }
log_ai_response() { echo -e "\033[1;32m🤖 ASİSTAN:\033[0m $1"; }

# Tek bir konuşma turunu yöneten ana fonksiyon
chat_turn() {
    local user_input="$1"
    local system_prompt_override="${2:-$DEFAULT_SYSTEM_PROMPT}"

    jq --arg content "$user_input" '. += [{"role": "user", "content": $content}]' "$HISTORY_FILE" > "${HISTORY_FILE}.tmp" && mv "${HISTORY_FILE}.tmp" "$HISTORY_FILE"

    local PAYLOAD
    PAYLOAD=$(jq -n --arg rag "$RAG_DATA" --arg sys "$system_prompt_override" --slurpfile hist "$HISTORY_FILE" \
        '{ "messages": ([{"role":"system","content":$sys}] + $hist[0]), "rag_context": $rag, "temperature": 0.0, "max_tokens": 150}')

    local full_response
    full_response=$(send_chat "$PAYLOAD" | jq -r '.choices[0].message.content')
    
    log_ai_response "$full_response"
    jq --arg content "$full_response" '. += [{"role": "assistant", "content": $content}]' "$HISTORY_FILE" > "${HISTORY_FILE}.tmp" && mv "${HISTORY_FILE}.tmp" "$HISTORY_FILE"
}

# --- TEST AKIŞI ---

# 1. Başlangıç
echo "[]" > "$HISTORY_FILE"

# 2. Karmaşık İlk Soru (Hem sipariş hem ajanda)
log_user "Siparişim ne durumda ve bugün başka bir işim var mı?"
jq --arg c "Siparişim ne durumda ve bugün başka bir işim var mı?" '. += [{"role": "user", "content": $c}]' "$HISTORY_FILE" > "${HISTORY_FILE}.tmp" && mv "${HISTORY_FILE}.tmp" "$HISTORY_FILE"
PAYLOAD=$(jq -n --arg rag "$RAG_DATA" --arg sys "$DEFAULT_SYSTEM_PROMPT" --slurpfile hist "$HISTORY_FILE" \
    '{ "messages": $hist[0], "rag_context": $rag, "stream": true, "temperature": 0.0 }')

log_gateway "LLM'e uzun bir cevap vermesi için karmaşık soru gönderiliyor..."
echo -n -e "\033[1;32m🤖 ASİSTAN:\033[0m "

# Coproc ile %100 kararlı arka plan işlemi
coproc LLM_STREAM {
    curl -s -N -X POST "$API_URL/v1/chat/completions" -d "$PAYLOAD" -H "Content-Type: application/json" | 
    while IFS= read -r line; do
        if [[ $line == "data: "* && $line != *"DONE"* ]]; then
            echo "${line:6}" | jq -r '.choices[0].delta.content // empty'
        fi
    done
}

# AI çıktısını 2 saniye boyunca oku ve kaydet
PARTIAL_RESPONSE=""
while IFS= read -r -t 2 token; do
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

# 4. YENİ ve ALAKASIZ SORU
chat_turn "Aaa pardon, dişçi randevum saat kaçtaydı?"

# 5. HAFİZA TESTİ (Recency Bias Tuzağı)
MEMORY_PROMPT="Sen bir analizcisin. Sana verilen konuşma geçmişini incele ve kullanıcının en baştaki ilk sorusunu (sipariş durumu ve ajanda) hatırlayıp özetle."
chat_turn "Tamamdır. Peki en başta ne konuşuyorduk, konuyu dağıttım." "$MEMORY_PROMPT"

# 6. SON KONTROL
chat_turn "Yarınki toplantı saat kaçta?"

log_header "Nihai Simülasyon Tamamlandı."
rm "$HISTORY_FILE"
#!/bin/bash
source tests/lib/common.sh
HISTORY_FILE="/tmp/personal_flow.json"; echo "[]" > "$HISTORY_FILE"

# RAG Data: 'Yarın Sabah' ifadesi eklendi.
RAG_DATA="Kullanıcı: Emre. AJANDA: [Bugün 15:00 Dişçi], [Bugün 18:00 Spor]. [Yarın Sabah 09:00 Toplantı]. NOT: Marketten SÜT al."

log_header "SENARYO: Kişisel Asistan (Samimi ve Yardımcı)"

chat_turn() {
    local input=$1; local key=$2; local step=$3
    echo -e "\n🔹 [$step] Kullanıcı: $input"
    jq --arg c "$input" '. += [{"role": "user", "content": $c}]' "$HISTORY_FILE" > "${HISTORY_FILE}.tmp" && mv "${HISTORY_FILE}.tmp" "$HISTORY_FILE"
    
    PAYLOAD=$(jq -n --arg rag "$RAG_DATA" --slurpfile hist "$HISTORY_FILE" \
    --arg sys "Sen Emre'nin asistanısın. Samimi konuş. Ajanda ve notları RAG verisinden bul. Yarınki işleri sorarsa söyle." \
    '{messages: ([{"role":"system","content":$sys}] + $hist[0]), rag_context: $rag, temperature: 0.0, max_tokens: 150}')
    
    RES=$(send_chat "$PAYLOAD" | jq -r '.choices[0].message.content' | sed 's/<think>.*<\/think>//g' | tr -d '\n')
    echo -e "🤖 AI: $RES"
    jq --arg c "$RES" '. += [{"role": "assistant", "content": $c}]' "$HISTORY_FILE" > "${HISTORY_FILE}.tmp" && mv "${HISTORY_FILE}.tmp" "$HISTORY_FILE"
    
    if echo "$RES" | grep -iqE "$key"; then log_pass "$step Başarılı"; else log_fail "$step Başarısız! Beklenen: $key"; fi
}

chat_turn "Bugün nelerim var?" "dişçi|spor" "Gündem Özeti"
chat_turn "Eve dönerken markete uğrayacaktım, ne alacaktım?" "süt" "Not Hatırlatma"
# 'evet' cevabı da kabul edilir
chat_turn "Yarın sabah işim var mı?" "toplantı|evet" "Yarın Kontrolü"
rm "$HISTORY_FILE"
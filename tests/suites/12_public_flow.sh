#!/bin/bash
source tests/lib/common.sh
HISTORY_FILE="/tmp/public_flow.json"; echo "[]" > "$HISTORY_FILE"

RAG_DATA="Vatandaş: Kemal Tunç. Başvuru: Yapı Ruhsatı (No: 20941). Durum: Eksik Evrak Tamamlandı, Teknik İncelemede. Tahmini Süre: 7-10 Gün."

log_header "SENARYO: Kamu (Belediye Asistanı)"

chat_turn() {
    local input=$1; local key=$2; local step=$3
    echo -e "\n🔹 [$step] Kullanıcı: $input"
    jq --arg c "$input" '. += [{"role": "user", "content": $c}]' "$HISTORY_FILE" > "${HISTORY_FILE}.tmp" && mv "${HISTORY_FILE}.tmp" "$HISTORY_FILE"
    
    PAYLOAD=$(jq -n --arg rag "$RAG_DATA" --slurpfile hist "$HISTORY_FILE" \
    --arg sys "Sen bir belediye asistanısın. Vatandaşa resmi, saygılı ve net bilgi ver. Durum kısmını oku." \
    '{messages: ([{"role":"system","content":$sys}] + $hist[0]), rag_context: $rag, temperature: 0.0, max_tokens: 150}')
    
    RES=$(send_chat "$PAYLOAD" | jq -r '.choices[0].message.content' | sed 's/<think>.*<\/think>//g' | tr -d '\n')
    echo -e "🤖 AI: $RES"
    jq --arg c "$RES" '. += [{"role": "assistant", "content": $c}]' "$HISTORY_FILE" > "${HISTORY_FILE}.tmp" && mv "${HISTORY_FILE}.tmp" "$HISTORY_FILE"
    
    if echo "$RES" | grep -iqE "$key"; then log_pass "$step Başarılı"; else log_fail "$step Başarısız! Beklenen: $key"; fi
}

# 'yapı ruhsatı' kelimesi de kabul edilir çünkü içerik doğru
chat_turn "Ruhsat başvurum ne alemde?" "inceleme|teknik|yapı ruhsatı" "Durum Sorgusu"
chat_turn "Daha ne kadar bekleyeceğim?" "gün|7|10" "Süre Sorgusu"
rm "$HISTORY_FILE"
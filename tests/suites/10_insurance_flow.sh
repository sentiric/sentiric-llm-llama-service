#!/bin/bash
source tests/lib/common.sh
HISTORY_FILE="/tmp/insurance_flow.json"; echo "[]" > "$HISTORY_FILE"

RAG_DATA="Sigortalı: Mehmet Aslan. Poliçe: Kasko. Durum: Aktif. Hasar Dosyası: Açıldı. Durum: Eksper raporu bekleniyor. TAHMİNİ SONUÇLANMA SÜRESİ: 3 iş günü."

log_header "SENARYO: Sigorta (Hasar Danışmanı)"

chat_turn() {
    local input=$1; local key=$2; local step=$3
    echo -e "\n🔹 [$step] Kullanıcı: $input"
    jq --arg c "$input" '. += [{"role": "user", "content": $c}]' "$HISTORY_FILE" > "${HISTORY_FILE}.tmp" && mv "${HISTORY_FILE}.tmp" "$HISTORY_FILE"
    
    PAYLOAD=$(jq -n --arg rag "$RAG_DATA" --slurpfile hist "$HISTORY_FILE" \
    --arg sys "Sen bir sigorta danışmanısın. RAG verisinden 'TAHMİNİ SONUÇLANMA SÜRESİ' bilgisini oku ve müşteriye söyle." \
    '{messages: ([{"role":"system","content":$sys}] + $hist[0]), rag_context: $rag, temperature: 0.0, max_tokens: 150}')
    
    RES=$(send_chat "$PAYLOAD" | jq -r '.choices[0].message.content' | sed 's/<think>.*<\/think>//g' | tr -d '\n')
    echo -e "🤖 AI: $RES"
    jq --arg c "$RES" '. += [{"role": "assistant", "content": $c}]' "$HISTORY_FILE" > "${HISTORY_FILE}.tmp" && mv "${HISTORY_FILE}.tmp" "$HISTORY_FILE"
    
    if echo "$RES" | grep -iqE "$key"; then log_pass "$step Başarılı"; else log_fail "$step Başarısız! Beklenen: $key"; fi
}

chat_turn "Arabanın tamiri ne durumda?" "eksper|rapor|bekleniyor" "Dosya Durumu"
chat_turn "Ne zaman sonuçlanır peki?" "3|gün" "Süre Bilgisi"
rm "$HISTORY_FILE"
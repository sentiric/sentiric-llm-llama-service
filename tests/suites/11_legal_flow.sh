#!/bin/bash
source tests/lib/common.sh
HISTORY_FILE="/tmp/legal_flow.json"; echo "[]" > "$HISTORY_FILE"

RAG_DATA="Müvekkil: Derya Kocaman. Dava: İş Mahkemesi. SON DURUM: DOSYAYA YENİ BİLİRKİŞİ RAPORU GİRDİ. RAPOR SONUCU OLUMLU (LEHE). Duruşma: 5 Aralık 2025, Saat 10:00."

log_header "SENARYO: Hukuk (Avukat Asistanı)"

chat_turn() {
    local input=$1; local key=$2; local step=$3
    echo -e "\n🔹 [$step] Kullanıcı: $input"
    jq --arg c "$input" '. += [{"role": "user", "content": $c}]' "$HISTORY_FILE" > "${HISTORY_FILE}.tmp" && mv "${HISTORY_FILE}.tmp" "$HISTORY_FILE"
    
    # Prompt: 'Yok' kelimesi yasaklandı.
    PAYLOAD=$(jq -n --arg rag "$RAG_DATA" --slurpfile hist "$HISTORY_FILE" \
    --arg sys "Sen bir hukuk asistanısın. RAG verisindeki 'SON DURUM' bilgisini mutlaka oku. ASLA 'Yok' veya 'Hayır' deme, raporun içeriğini anlat." \
    '{messages: ([{"role":"system","content":$sys}] + $hist[0]), rag_context: $rag, temperature: 0.0, max_tokens: 150}')
    
    RES=$(send_chat "$PAYLOAD" | jq -r '.choices[0].message.content' | sed 's/<think>.*<\/think>//g' | tr -d '\n')
    echo -e "🤖 AI: $RES"
    jq --arg c "$RES" '. += [{"role": "assistant", "content": $c}]' "$HISTORY_FILE" > "${HISTORY_FILE}.tmp" && mv "${HISTORY_FILE}.tmp" "$HISTORY_FILE"
    
    if echo "$RES" | grep -iqE "$key"; then log_pass "$step Başarılı"; else log_fail "$step Başarısız! Beklenen: $key"; fi
}

chat_turn "Dosyada bilirkişi raporu geldi mi, gelişme var mı?" "bilirkişi|lehe|olumlu|rapor|geldi" "Gelişme Sorgusu"
chat_turn "Bir sonraki mahkeme ne zaman?" "5 Aralık" "Tarih Sorgusu"
rm "$HISTORY_FILE"
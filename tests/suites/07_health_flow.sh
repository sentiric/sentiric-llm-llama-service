#!/bin/bash
source tests/lib/common.sh
HISTORY_FILE="/tmp/health_flow.json"; echo "[]" > "$HISTORY_FILE"

RAG_DATA="Hasta: Ayşe Demir. Randevu: Kardiyoloji, Dr. Mehmet Öz. Tarih: Yarın Saat 14:00. Durum: Onaylı. Not: Hasta kan sulandırıcı kullanıyor."

log_header "SENARYO: Sağlık Asistanı Akışı (Hassasiyet ve Teyit)"

chat_turn() {
    local input=$1; local key=$2; local step=$3
    echo -e "\n🔹 [$step] Kullanıcı: $input"
    jq --arg c "$input" '. += [{"role": "user", "content": $c}]' "$HISTORY_FILE" > "${HISTORY_FILE}.tmp" && mv "${HISTORY_FILE}.tmp" "$HISTORY_FILE"
    
    # Prompt Güçlendirildi: Saat formatı ve Acil Durum vurgusu.
    PAYLOAD=$(jq -n --arg rag "$RAG_DATA" --slurpfile hist "$HISTORY_FILE" \
    --arg sys "Sen bir hastane randevu asistanısın. ASLA tıbbi tavsiye verme. Acil durumlarda (ağrı, kanama vb.) DERHAL 'Acil Servise' veya 'Doktora' yönlendir. Randevu saatini tam olarak (örn: 14:00) söyle." \
    '{messages: ([{"role":"system","content":$sys}] + $hist[0]), rag_context: $rag, temperature: 0.1, max_tokens: 150}')
    
    RES=$(send_chat "$PAYLOAD" | jq -r '.choices[0].message.content' | sed 's/<think>.*<\/think>//g' | tr -d '\n')
    echo -e "🤖 AI: $RES"
    jq --arg c "$RES" '. += [{"role": "assistant", "content": $c}]' "$HISTORY_FILE" > "${HISTORY_FILE}.tmp" && mv "${HISTORY_FILE}.tmp" "$HISTORY_FILE"
    
    if echo "$RES" | grep -iqE "$key"; then log_pass "$step Başarılı"; else log_fail "$step Başarısız! Beklenen: $key"; fi
}

chat_turn "Yarınki randevum kaçtaydı?" "14:00|14.00" "Saat Sorgusu"
chat_turn "Göğsümde hafif bir ağrı var, korkuyorum." "doktor|acil|hastane|112" "Acil Durum Yönlendirmesi"
chat_turn "Doktor benim ilaç kullandığımı biliyor mu?" "biliyor|evet|sulandırıcı" "Bağlam Kontrolü"
chat_turn "Doktorun adı neydi?" "mehmet" "Hafıza Testi"
rm "$HISTORY_FILE"
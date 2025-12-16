#!/bin/bash
source tests/lib/common.sh
HISTORY_FILE="/tmp/finance_flow.json"; echo "[]" > "$HISTORY_FILE"

# RAG Data: "Yok" ifadesi vurgulandı.
RAG_DATA="Müşteri: Ece Çetin. Bakiye: 45.000 TL. Son İşlem: 14 Kasım, 12.500 TL Havale (Alıcı: Caner Yıldız). Hesap Türü: Vadesiz TL. Döviz Hesabı: MEVCUT DEĞİL (YOK)."

log_header "SENARYO: Finans (Banka Asistanı)"

chat_turn() {
    local input=$1; local key=$2; local step=$3
    echo -e "\n🔹 [$step] Kullanıcı: $input"
    jq --arg c "$input" '. += [{"role": "user", "content": $c}]' "$HISTORY_FILE" > "${HISTORY_FILE}.tmp" && mv "${HISTORY_FILE}.tmp" "$HISTORY_FILE"
    
    # Prompt Güçlendirildi: Halüsinasyon önleme.
    PAYLOAD=$(jq -n --arg rag "$RAG_DATA" --slurpfile hist "$HISTORY_FILE" \
    --arg sys "Sen ciddi bir banka asistanısın. Sadece RAG verisindeki bilgileri kullan. Veride 'Yok' yazıyorsa 'Yok' de, asla uydurma." \
    '{messages: ([{"role":"system","content":$sys}] + $hist[0]), rag_context: $rag, temperature: 0.1, max_tokens: 150}')
    
    RES=$(send_chat "$PAYLOAD" | jq -r '.choices[0].message.content' | sed 's/<think>.*<\/think>//g' | tr -d '\n')
    echo -e "🤖 AI: $RES"
    jq --arg c "$RES" '. += [{"role": "assistant", "content": $c}]' "$HISTORY_FILE" > "${HISTORY_FILE}.tmp" && mv "${HISTORY_FILE}.tmp" "$HISTORY_FILE"
    
    if echo "$RES" | grep -iqE "$key"; then log_pass "$step Başarılı"; else log_fail "$step Başarısız! Beklenen: $key"; fi
}

chat_turn "Son yaptığım işlem kime gitti?" "caner|yıldız" "İşlem Detayı"
chat_turn "Hesabımda ne kadar kaldı?" "45|bin" "Bakiye Sorgusu"
chat_turn "Dolar almak istiyorum, hesabım var mı?" "yok|mevcut değil|açalım" "Çapraz Satış/Bilgi"
rm "$HISTORY_FILE"
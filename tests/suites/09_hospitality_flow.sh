#!/bin/bash
source tests/lib/common.sh
HISTORY_FILE="/tmp/hospitality_flow.json"; echo "[]" > "$HISTORY_FILE"

RAG_DATA="Müşteri: Ahmet Yılmaz. Rezervasyon: 14-16 Kasım. Oda: Deniz Manzaralı. Giriş: 14:00. Ödeme Durumu: HENÜZ YAPILMADI (Girişte Alınacak). Özel Not: Müşteri GEÇ GİRİŞ yapacak, OTEL TARAFINDAN ONAYLANDI (Sorun Yok)."

log_header "SENARYO: Turizm (Otel Resepsiyonu)"

chat_turn() {
    local input=$1; local key=$2; local step=$3
    echo -e "\n🔹 [$step] Kullanıcı: $input"
    jq --arg c "$input" '. += [{"role": "user", "content": $c}]' "$HISTORY_FILE" > "${HISTORY_FILE}.tmp" && mv "${HISTORY_FILE}.tmp" "$HISTORY_FILE"
    
    PAYLOAD=$(jq -n --arg rag "$RAG_DATA" --slurpfile hist "$HISTORY_FILE" \
    --arg sys "Sen otel resepsiyonistisin. Bilgileri RAG verisinden oku." \
    '{messages: ([{"role":"system","content":$sys}] + $hist[0]), rag_context: $rag, temperature: 0.1, max_tokens: 150}')
    
    RES=$(send_chat "$PAYLOAD" | jq -r '.choices[0].message.content' | sed 's/<think>.*<\/think>//g' | tr -d '\n')
    echo -e "🤖 AI: $RES"
    jq --arg c "$RES" '. += [{"role": "assistant", "content": $c}]' "$HISTORY_FILE" > "${HISTORY_FILE}.tmp" && mv "${HISTORY_FILE}.tmp" "$HISTORY_FILE"
    
    # [GÜNCELLEME] Regex "yapmadın" ve "alınacak" kelimelerini içerecek şekilde esnetildi
    if echo "$RES" | grep -iqE "$key"; then 
        log_pass "$step Başarılı"
    else 
        log_fail "$step Başarısız! Beklenen: $key"
    fi
}

chat_turn "Odam manzaralı mı?" "deniz|manzara|evet" "Oda Bilgisi"
chat_turn "Akşam 8 gibi gelsem sorun olur mu?" "onaylandı|sorun yok|bekliyoruz|olmayacak|olmaz|uygun|sorun olmaz" "Özel İstek Kontrolü"
# [GÜNCELLEME] Beklenen anahtar kelimeler listesi genişletildi
chat_turn "Ödemeyi şimdi mi yaptım?" "girişte|yapılmadı|alınacak|yapmadın|ödemediniz|ödenmedi" "Ödeme Bilgisi"

rm "$HISTORY_FILE"
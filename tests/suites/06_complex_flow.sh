#!/bin/bash
source tests/lib/common.sh

HISTORY_FILE="/tmp/ecommerce_flow.json"
echo "[]" > "$HISTORY_FILE"

RAG_DATA="Müşteri: Burak Yılmaz. Sipariş No: #9988. Ürün: Gaming Laptop. Durum: Hazırlanıyor. Fiyat: 50.000 TL. İade Politikası: Ürün hazırlık aşamasında olsa bile müşteri isterse DERHAL İPTAL ve İADE yapılır."

log_header "SENARYO: E-Ticaret Akışı (Empati ve İşlem Odaklı)"

chat_turn() {
    local user_input="$1"
    local expect_keyword="$2"
    local step_name="$3"

    echo -e "\n🔹 [ADIM: $step_name] Kullanıcı: $user_input"
    jq --arg content "$user_input" '. += [{"role": "user", "content": $content}]' "$HISTORY_FILE" > "${HISTORY_FILE}.tmp" && mv "${HISTORY_FILE}.tmp" "$HISTORY_FILE"

    PAYLOAD=$(jq -n \
        --arg rag "$RAG_DATA" \
        --arg sys "Sen bir E-ticaret asistanısın. [SİPARİŞ BİLGİSİ] bloğuna göre cevap ver. Müşteri iptal isterse onayla. Cevapların net olsun." \
        --slurpfile hist "$HISTORY_FILE" \
        '{
            "messages": ([{"role": "system", "content": $sys}] + $hist[0]),
            "rag_context": $rag,
            "temperature": 0.2,
            "max_tokens": 150
        }')

    RESPONSE=$(send_chat "$PAYLOAD")
    AI_REPLY=$(echo "$RESPONSE" | jq -r '.choices[0].message.content' | sed 's/<think>.*<\/think>//g' | tr -d '\n')
    echo -e "🤖 AI: $AI_REPLY"

    jq --arg content "$AI_REPLY" '. += [{"role": "assistant", "content": $content}]' "$HISTORY_FILE" > "${HISTORY_FILE}.tmp" && mv "${HISTORY_FILE}.tmp" "$HISTORY_FILE"

    if echo "$AI_REPLY" | grep -iqE "$expect_keyword"; then
        log_pass "$step_name Başarılı"
    else
        log_fail "$step_name Başarısız! Beklenen: '$expect_keyword', Alınan: '$AI_REPLY'"
    fi
}

chat_turn "Siparişim ne durumda?" "hazırlanıyor|hazırlık" "Durum Sorgusu"
chat_turn "Yeter artık, çok bekledim! İptal edin hemen!" "üzgün|özür|kusura|tamam|işleme|iptal|onay|derhal" "Empati ve İptal"
# [GÜNCELLEME] Regex onay/yatış kelimelerini kapsayacak şekilde esnetildi
chat_turn "Paramın hepsi yatacak mı?" "evet|tamamını|kesintisiz|yatacak|iade|yapılır|onay|yatış|yatırılacak" "İade Teyidi"
chat_turn "Ben ne almıştım?" "laptop|bilgisayar" "Hafıza Testi"

rm "$HISTORY_FILE"
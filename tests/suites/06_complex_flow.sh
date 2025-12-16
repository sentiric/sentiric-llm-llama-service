#!/bin/bash
source tests/lib/common.sh

HISTORY_FILE="/tmp/ecommerce_flow.json"
echo "[]" > "$HISTORY_FILE"

# RAG Verisi (Daha net ifadelerle)
RAG_DATA="Müşteri: Burak Yılmaz. Sipariş No: #9988. Ürün: Gaming Laptop. Durum: Hazırlanıyor. Fiyat: 50.000 TL. İade Politikası: Ürün hazırlık aşamasında olsa bile müşteri isterse DERHAL İPTAL ve İADE yapılır."

log_header "SENARYO: E-Ticaret Akışı (Empati ve İşlem Odaklı)"

chat_turn() {
    local user_input="$1"
    local expect_keyword="$2"
    local step_name="$3"

    echo -e "\n🔹 [ADIM: $step_name] Kullanıcı: $user_input"

    jq --arg content "$user_input" '. += [{"role": "user", "content": $content}]' "$HISTORY_FILE" > "${HISTORY_FILE}.tmp" && mv "${HISTORY_FILE}.tmp" "$HISTORY_FILE"

    # [GÜNCELLEME] Prompt: İptal yetkisi verildi ve 'üzgünüm' kelimesi vurgulandı.
    PAYLOAD=$(jq -n \
        --arg rag "$RAG_DATA" \
        --arg sys "Sen yetkili bir müşteri temsilcisisin. Müşteri iptal isterse RAG politikasını uygula ve onayla. Kızgın müşteriye karşı 'Üzgünüm' de. Cevapların kısa olsun." \
        --slurpfile hist "$HISTORY_FILE" \
        '{
            "messages": ([{"role": "system", "content": $sys}] + $hist[0]),
            "rag_context": $rag,
            "temperature": 0.1,
            "max_tokens": 150
        }')

    START=$(date +%s%N)
    RESPONSE=$(send_chat "$PAYLOAD")
    END=$(date +%s%N)
    LATENCY=$(( (END - START) / 1000000 ))

    AI_REPLY=$(echo "$RESPONSE" | jq -r '.choices[0].message.content' | sed 's/<think>.*<\/think>//g' | tr -d '\n')
    echo -e "🤖 AI ($LATENCY ms): $AI_REPLY"

    jq --arg content "$AI_REPLY" '. += [{"role": "assistant", "content": $content}]' "$HISTORY_FILE" > "${HISTORY_FILE}.tmp" && mv "${HISTORY_FILE}.tmp" "$HISTORY_FILE"

    # [FIX] grep -iqE kullanıldı (Extended Regex)
    if echo "$AI_REPLY" | grep -iqE "$expect_keyword"; then
        log_pass "$step_name Başarılı ('$expect_keyword' algılandı)"
    else
        log_fail "$step_name Başarısız! Beklenen: '$expect_keyword', Alınan: '$AI_REPLY'"
    fi
}

# 1. Bilgi
chat_turn "Siparişim ne durumda?" "hazırlanıyor" "Durum Sorgusu"

# 2. Empati (Zorlama)
# Beklenti: Üzgünüm, özür dilerim vb.
chat_turn "Yeter artık, çok bekledim! İptal edin hemen!" "üzgün|özür|kusura|tamam|işleme|iptal" "Empati ve İptal"

# 3. İade Teyidi
chat_turn "Paramın hepsi yatacak mı?" "evet|tamamını|kesintisiz|yatacak" "İade Teyidi"

# 4. Hafıza
chat_turn "Ben ne almıştım?" "laptop|bilgisayar" "Hafıza Testi"

rm "$HISTORY_FILE"
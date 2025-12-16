#!/bin/bash
source tests/lib/common.sh

CRM_DATA="Müşteri Adı: Ayşe Yılmaz. Borç: 1500 TL. Son Ödeme: Yarın. İnternet Paketi: Fiber 100Mbps (Arızalı)."
HISTORY_FILE="/tmp/phone_test_hist.json"

echo "[]" > "$HISTORY_FILE"

log_header "SENARYO: Telefon Asistanı Simülasyonu (Doğru Bağlam Kalıcılığı)"

# Bir RAG diyaloğunda, context her zaman mevcuttur.
# Bu fonksiyon, bu gerçek dünya senaryosunu doğru bir şekilde simüle eder.
talk() {
    local user_msg="$1"
    local expect_keyword="$2"
    
    # 1. Kullanıcının yeni mesajını geçmişe ekle
    jq --arg msg "$user_msg" '. += [{"role":"user", "content":$msg}]' "$HISTORY_FILE" > "${HISTORY_FILE}.tmp" && mv "${HISTORY_FILE}.tmp" "$HISTORY_FILE"
    
    # 2. Her zaman tam konuşma geçmişini ve RAG context'ini içeren bir payload oluştur
    PAYLOAD=$(jq -n \
        --arg rag "$CRM_DATA" \
        --slurpfile hist "$HISTORY_FILE" \
        '{
            "messages": $hist[0],
            "rag_context": $rag,
            "temperature": 0.1,
            "max_tokens": 150
        }')

    # 3. İsteği gönder ve doğrula
    START=$(date +%s%N)
    RESPONSE=$(send_chat "$PAYLOAD")
    END=$(date +%s%N)
    LATENCY=$(( (END - START) / 1000000 ))
    
    RAW_CONTENT=$(echo "$RESPONSE" | jq -r '.choices[0].message.content')
    CLEAN_CONTENT=$(echo "$RAW_CONTENT" | perl -0777 -pe 's/<think>.*?<\/think>//gs' | sed 's/<[^>]*>//g' | tr -s ' ' | sed 's/^[ \t]*//;s/[ \t]*$//')

    echo -e "👤 User: $user_msg"
    echo -e "🤖 AI (Clean): $CLEAN_CONTENT"
    echo -e "⏱️  Latency: ${LATENCY}ms"

    # [FIX] Case-insensitive grep (-i) eklendi
    if echo "$CLEAN_CONTENT" | grep -Fqi "$expect_keyword"; then
        log_pass "Cevap doğrulandı ('$expect_keyword' bulundu)."
    else
        log_fail "Beklenen bilgi eksik: '$expect_keyword'"
        return 1
    fi

    # 4. AI'ın cevabını bir sonraki tur için geçmişe ekle
    jq --arg msg "$CLEAN_CONTENT" '. += [{"role":"assistant", "content":$msg}]' "$HISTORY_FILE" > "${HISTORY_FILE}.tmp" && mv "${HISTORY_FILE}.tmp" "$HISTORY_FILE"
}


# 1. Aşama: Bilgi Al (RAG ile)
talk "Merhaba, borcum ne kadar?" "1500" || exit 1

# 2. Aşama: Bilgi Üzerine Konuş (RAG ile devam ederek)
talk "Son ödeme tarihi ne zaman peki?" "Yarın" || exit 1
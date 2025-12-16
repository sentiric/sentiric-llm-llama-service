#!/bin/bash
source tests/lib/common.sh

CRM_DATA="Müşteri Adı: Ayşe Yılmaz. Borç: 1500 TL. Son Ödeme: Yarın. İnternet Paketi: Fiber 100Mbps (Arızalı)."
HISTORY_FILE="/tmp/phone_test_hist.json"

echo "[]" > "$HISTORY_FILE"

log_header "SENARYO: Telefon Asistanı Simülasyonu"

# Sadece konuşmanın ilk adımı için RAG context'i ile istek gönderir.
talk_with_rag() {
    local user_msg="$1"
    local expect_keyword="$2"
    local rag_data="$3"

    jq --arg msg "$user_msg" '. += [{"role":"user", "content":$msg}]' "$HISTORY_FILE" > "${HISTORY_FILE}.tmp" && mv "${HISTORY_FILE}.tmp" "$HISTORY_FILE"
    
    PAYLOAD=$(jq -n \
        --arg rag "$rag_data" \
        --slurpfile hist "$HISTORY_FILE" \
        '{
            "messages": $hist[0],
            "rag_context": $rag,
            "temperature": 0.0,
            "max_tokens": 150
        }')

    send_and_validate "$user_msg" "$expect_keyword" "$PAYLOAD"
}

# RAG context'i OLMADAN, sadece konuşma geçmişiyle devam eder.
talk() {
    local user_msg="$1"
    local expect_keyword="$2"
    
    jq --arg msg "$user_msg" '. += [{"role":"user", "content":$msg}]' "$HISTORY_FILE" > "${HISTORY_FILE}.tmp" && mv "${HISTORY_FILE}.tmp" "$HISTORY_FILE"
    
    PAYLOAD=$(jq -n \
        --slurpfile hist "$HISTORY_FILE" \
        '{
            "messages": $hist[0],
            "temperature": 0.0,
            "max_tokens": 150
        }')

    send_and_validate "$user_msg" "$expect_keyword" "$PAYLOAD"
}

# Ortak istek gönderme ve doğrulama mantığı
send_and_validate() {
    local user_msg="$1"
    local expect_keyword="$2"
    local payload="$3"
    
    START=$(date +%s%N)
    RESPONSE=$(send_chat "$payload")
    END=$(date +%s%N)
    LATENCY=$(( (END - START) / 1000000 ))
    
    RAW_CONTENT=$(echo "$RESPONSE" | jq -r '.choices[0].message.content')
    CLEAN_CONTENT=$(echo "$RAW_CONTENT" | perl -0777 -pe 's/<think>.*?<\/think>//gs' | sed 's/<[^>]*>//g' | tr -s ' ' | sed 's/^[ \t]*//;s/[ \t]*$//')

    echo -e "👤 User: $user_msg"
    echo -e "🤖 AI (Clean): $CLEAN_CONTENT"
    echo -e "⏱️  Latency: ${LATENCY}ms"

    if echo "$CLEAN_CONTENT" | grep -Fq "$expect_keyword"; then
        log_pass "Cevap doğrulandı ('$expect_keyword' bulundu)."
    else
        log_fail "Beklenen bilgi eksik: '$expect_keyword'"
        return 1
    fi

    jq --arg msg "$CLEAN_CONTENT" '. += [{"role":"assistant", "content":$msg}]' "$HISTORY_FILE" > "${HISTORY_FILE}.tmp" && mv "${HISTORY_FILE}.tmp" "$HISTORY_FILE"
}


# 1. Aşama: Bilgi Al (RAG ile)
talk_with_rag "Merhaba, borcum ne kadar?" "1500" "$CRM_DATA" || exit 1

# 2. Aşama: Bilgi Üzerine Konuş (RAG olmadan)
talk "Son ödeme tarihi ne zaman peki?" "Yarın" || exit 1
#!/bin/bash
source tests/lib/common.sh

CRM_DATA="Müşteri Adı: Ayşe Yılmaz. Borç: 1500 TL. Son Ödeme: Yarın. İnternet Paketi: Fiber 100Mbps (Arızalı)."
HISTORY_FILE="/tmp/phone_test_hist.json"

echo "[]" > "$HISTORY_FILE"

log_header "SENARYO: Telefon Asistanı Simülasyonu"

talk() {
    local user_msg="$1"
    local expect_keyword="$2"
    
    jq --arg msg "$user_msg" '. += [{"role":"user", "content":$msg}]' "$HISTORY_FILE" > "${HISTORY_FILE}.tmp" && mv "${HISTORY_FILE}.tmp" "$HISTORY_FILE"
    
    PAYLOAD=$(jq -n \
        --arg rag "$CRM_DATA" \
        --slurpfile hist "$HISTORY_FILE" \
        '{
            messages: $hist[0],
            rag_context: $rag,
            temperature: 0.1,
            max_tokens: 150
        }')

    START=$(date +%s%N)
    RESPONSE=$(send_chat "$PAYLOAD")
    END=$(date +%s%N)
    LATENCY=$(( (END - START) / 1000000 ))
    
    CONTENT=$(echo "$RESPONSE" | jq -r '.choices[0].message.content')
    
    # <think> bloklarını temizle (Görsel temizlik için)
    CLEAN_CONTENT=$(echo "$CONTENT" | sed 's/<think>.*<\/think>//g' | sed 's/<thought>.*<\/thought>//g')
    
    echo -e "👤 User: $user_msg"
    echo -e "🤖 AI (Clean): $CLEAN_CONTENT"
    echo -e "⏱️  TTFT/Latency: ${LATENCY}ms"

    # Kontrolü RAW content üzerinden yap (bazen cevap think bloğu içinde sızabilir)
    if echo "$CONTENT" | grep -iq "$expect_keyword"; then
        log_pass "Cevap doğrulandı ('$expect_keyword' bulundu)."
    else
        log_fail "Beklenen bilgi eksik: '$expect_keyword'"
        return 1
    fi

    # History'ye temizlenmiş cevabı ekle (Chain of Thought context'i şişirmesin)
    jq --arg msg "$CLEAN_CONTENT" '. += [{"role":"assistant", "content":$msg}]' "$HISTORY_FILE" > "${HISTORY_FILE}.tmp" && mv "${HISTORY_FILE}.tmp" "$HISTORY_FILE"
}

# --- TEST ADIMLARI ---
talk "Merhaba, borcum ne kadar?" "1500" || exit 1
talk "Son ödeme tarihi ne zaman peki?" "Yarın" || exit 1
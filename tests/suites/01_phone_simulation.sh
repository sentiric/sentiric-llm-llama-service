#!/bin/bash
source tests/lib/common.sh

CRM_DATA="Müşteri Adı: Ayşe Yılmaz. Borç: 1500 TL. Son Ödeme: Yarın. İnternet Paketi: Fiber 100Mbps (Arızalı)."
HISTORY_FILE="/tmp/phone_test_hist.json"

echo "[]" > "$HISTORY_FILE"

log_header "SENARYO: Telefon Asistanı Simülasyonu"

# Helper: Konuşma döngüsü
talk() {
    local user_msg="$1"
    local expect_keyword="$2"
    
    # User mesajını history'ye ekle
    jq --arg msg "$user_msg" '. += [{"role":"user", "content":$msg}]' "$HISTORY_FILE" > "${HISTORY_FILE}.tmp" && mv "${HISTORY_FILE}.tmp" "$HISTORY_FILE"
    
    # Payload hazırla
    PAYLOAD=$(jq -n \
        --arg rag "$CRM_DATA" \
        --slurpfile hist "$HISTORY_FILE" \
        '{
            messages: $hist[0],
            rag_context: $rag,
            temperature: 0.1,
            max_tokens: 100
        }')

    START=$(date +%s%N)
    RESPONSE=$(send_chat "$PAYLOAD")
    END=$(date +%s%N)
    
    # Latency (ms)
    LATENCY=$(( (END - START) / 1000000 ))
    
    CONTENT=$(echo "$RESPONSE" | jq -r '.choices[0].message.content')
    
    echo -e "👤 User: $user_msg"
    echo -e "🤖 AI: $CONTENT"
    echo -e "⏱️  TTFT/Latency: ${LATENCY}ms"

    # Kontrol
    if echo "$CONTENT" | grep -iq "$expect_keyword"; then
        log_pass "Cevap doğrulandı ('$expect_keyword' bulundu)."
    else
        log_fail "Beklenen bilgi eksik: '$expect_keyword'"
        return 1
    fi

    # AI cevabını history'ye ekle
    jq --arg msg "$CONTENT" '. += [{"role":"assistant", "content":$msg}]' "$HISTORY_FILE" > "${HISTORY_FILE}.tmp" && mv "${HISTORY_FILE}.tmp" "$HISTORY_FILE"
}

# --- ADIM 1: Tanışma ve RAG Kontrolü ---
talk "Merhaba, borcum ne kadar?" "1500" || exit 1

# --- ADIM 2: Hafıza (Context) Kontrolü ---
talk "Son ödeme tarihi ne zaman peki?" "Yarın" || exit 1

# --- ADIM 3: RAG + Hafıza Kombinasyonu ---
talk "İnternetim neden çalışmıyor?" "Arıza" || exit 1
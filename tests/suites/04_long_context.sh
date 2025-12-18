#!/bin/bash
source tests/lib/common.sh

log_header "SENARYO: Uzun Bağlam ve Mantık Zorlama"

# 1. Karmaşık RAG Verisi (Uzun Metin)
LONG_CONTEXT="Sentiric Bulut Platformu Kullanım Şartları: 
Madde 1: Hizmet kesintisi durumunda, kesinti süresi 4 saati aşarsa %10, 12 saati aşarsa %25, 24 saati aşarsa %50 iade yapılır.
Madde 2: Kullanıcı verileri şifreli saklanır. Şifre anahtarı kullanıcıdadır.
Madde 3: Fatura itirazları 7 iş günü içinde yapılmalıdır. Aksi takdirde fatura kabul edilmiş sayılır.
Madde 4: Bakım çalışmaları en az 48 saat önceden bildirilir. Acil durumlar hariçtir."

# 2. Mantık Sorusu (Logic Reasoning)
USER_QUERY="Merhaba, dün sisteminiz 15 saat kesildi. Ne kadar iade alacağım?"

# [FIX] Prompt Engineering: "Adım adım düşün" ve "Maddeleri kontrol et" talimatı eklendi.
PAYLOAD=$(jq -n \
    --arg rag "$LONG_CONTEXT" \
    --arg msg "$USER_QUERY" \
    --arg sys "Sen bir hukuk asistanısın. RAG verisindeki 'Madde 1'i dikkatlice analiz et. Kesinti süresini maddelerle karşılaştır. Adım adım düşünerek doğru iade oranını bul." \
    '{
        "messages": [{"role": "system", "content": $sys}, {"role": "user", "content": $msg}],
        "rag_context": $rag,
        "temperature": 0.0,
        "max_tokens": 200
    }')

log_info "Soru: $USER_QUERY"

START=$(date +%s%N)
RESPONSE=$(send_chat "$PAYLOAD")
END=$(date +%s%N)
LATENCY=$(( (END - START) / 1000000 ))

RAW_CONTENT=$(echo "$RESPONSE" | jq -r '.choices[0].message.content')
# Düşünme taglerini temizle
CLEAN_CONTENT=$(echo "$RAW_CONTENT" | sed 's/<think>.*<\/think>//g' | tr -s ' ')

echo -e "🤖 AI Cevabı: $CLEAN_CONTENT"
echo -e "⏱️  Süre: ${LATENCY}ms"

# Beklenen: %25 (Çünkü 15 saat, 12 saati aşıyor ama 24 saati aşmıyor)
if echo "$CLEAN_CONTENT" | grep -q "%25"; then
    log_pass "Mantık Doğru: %25 iade hesaplandı."
else
    # Hata durumunda detaylı log
    log_fail "Mantık Hatası: Beklenen '%25', Alınan: '$CLEAN_CONTENT'"
    exit 1
fi
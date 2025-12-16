#!/bin/bash
source tests/lib/common.sh

log_header "SENARYO: Nihai Diyalog (GARANTİLİ Söz Kesme, Hafıza, Eskalasyon)"

# --- Test Ayarları ---
RAG_DATA="Müşteri: Zeynep Kaya. Sipariş No: #XYZ-789. Ürün: Kırmızı Elbigse. Durum: KARGOYA VERİLDİ. Kargo Takip: TRK-12345. İade Politikası: 14 gün içinde koşulsuz iade. Müşteri Notu: Hediye paketi istendi."
DEFAULT_SYSTEM_PROMPT="Sen bir müşteri hizmetleri temsilcisisin. Nazik ve yardımsever ol."
HISTORY_FILE="/tmp/adversarial_sim_history.json"

# --- Simülasyon Fonksiyonları ---
log_user() { echo -e "\n\033[1;34m🙍‍♀️ MÜŞTERİ:\033[0m $1"; }
log_gateway() { echo -e "\n\033[1;33m⚡ GATEWAY:\033[0m $1"; }

# Tek bir konuşma turunu yöneten ana fonksiyon
chat_turn() {
    local user_input="$1"
    local system_prompt_override="${2:-$DEFAULT_SYSTEM_PROMPT}"

    jq --arg c "$user_input" '. += [{"role": "user", "content": $c}]' "$HISTORY_FILE" > "${HISTORY_FILE}.tmp" && mv "${HISTORY_FILE}.tmp" "$HISTORY_FILE"

    local PAYLOAD
    PAYLOAD=$(jq -n --arg rag "$RAG_DATA" --arg sys "$system_prompt_override" --slurpfile hist "$HISTORY_FILE" \
        '{ "messages": ([{"role":"system","content":$sys}] + $hist[0]), "rag_context": $rag, "temperature": 0.1, "max_tokens": 100 }')

    local full_response
    full_response=$(send_chat "$PAYLOAD" | jq -r '.choices[0].message.content')
    
    echo -n -e "\033[1;32m🤖 ASİSTAN:\033[0m "
    echo "$full_response"
    jq --arg c "$full_response" '. += [{"role": "assistant", "content": $c}]' "$HISTORY_FILE" > "${HISTORY_FILE}.tmp" && mv "${HISTORY_FILE}.tmp" "$HISTORY_FILE"
}

# --- TEST AKIŞI ---

# 1. Başlangıç
echo "[]" > "$HISTORY_FILE"

# 2. İlk Soru
log_user "Merhaba, siparişim hakkında bana TÜM detayları anlatır mısın?"
jq --arg c "Merhaba, siparişim hakkında bana TÜM detayları anlatır mısın?" '. += [{"role": "user", "content": $c}]' "$HISTORY_FILE" > "${HISTORY_FILE}.tmp" && mv "${HISTORY_FILE}.tmp" "$HISTORY_FILE"
LONG_RESPONSE_PROMPT="Sen çok konuşkan bir asistansın. Müşteriye SİPARİŞ NUMARASI, ÜRÜN, DURUM, KARGO BİLGİSİ ve İADE POLİTİKASINI uzun cümlelerle, detaylıca anlat."
PAYLOAD=$(jq -n --arg rag "$RAG_DATA" --arg sys "$LONG_RESPONSE_PROMPT" --slurpfile hist "$HISTORY_FILE" \
    '{ "messages": $hist[0], "rag_context": $rag, "stream": true, "temperature": 0.2, "max_tokens": 300 }')

log_gateway "LLM'e uzun bir cevap vermesi için istek gönderiliyor..."
echo -n -e "\033[1;32m🤖 ASİSTAN:\033[0m "

# [FİNAL DÜZELTME] Coproc ile güvenli arka plan işlemi
coproc LLM_STREAM {
    curl -s -N -X POST "$API_URL/v1/chat/completions" -d "$PAYLOAD" -H "Content-Type: application/json" | 
    while IFS= read -r line; do
        if [[ $line == "data: "* && $line != *"DONE"* ]]; then
            echo "${line:6}" | jq -r '.choices[0].delta.content // empty'
        fi
    done
}

PARTIAL_RESPONSE=""
interrupted=false
# [FİNAL DÜZELTME] Döngü, pipe kapanınca kendi kendine sonlanacak.
while IFS= read -r token; do
    if [ -n "$token" ]; then
        echo -n -e "\033[0;32m$token\033[0m"
        PARTIAL_RESPONSE+="$token"
    fi

    if [ "$interrupted" = false ]; then
        log_gateway "!!! VAD: MÜŞTERİ ARAYA GİRDİ !!!"
        kill $LLM_STREAM_PID 2>/dev/null
        interrupted=true
        log_gateway "-> LLM stream kesildi."
    fi
done <&"${LLM_STREAM[0]}"
wait $LLM_STREAM_PID 2>/dev/null

jq --arg c "$PARTIAL_RESPONSE" '. += [{"role": "assistant", "content": $c}]' "$HISTORY_FILE" > "${HISTORY_FILE}.tmp" && mv "${HISTORY_FILE}.tmp" "$HISTORY_FILE"
log_gateway "-> AI'ın yarım cümlesi hafızaya kaydedildi: \"$PARTIAL_RESPONSE...\""

# 4. ŞİKAYET & KONU DEĞİŞİMİ
chat_turn "Dur dur! Hediye paketi istedim ben, o ne oldu? Söylemedin!" "Sen bir asistansın. Müşterinin hediye paketi notunu RAG verisinden bul ve onayla."

# 5. HAFIZA KAYBI NUMARASI YAPAN MÜŞTERİ
chat_turn "Tamam, peki kargo numarasını tekrar söyler misin? Az önce söylemeye başladın ama kaçırdım."

# 6. ESKALASYON (Yetkili İsteği)
chat_turn "Ben sizinle anlaşamıyorum, sürekli aynı şeyleri soruyorum. Lütfen beni bir yetkiliye bağlayın." "Sen bir asistansın. Yetkili taleplerini anladığını belirt ve 'ilgili birime aktarıyorum' de. Asla 'bağlayamam' deme."

# 7. NİHAİ HAFIZA TESTİ
chat_turn "Peki, tüm bu karışıklıktan önce, en başta ne için aramıştım?" "Sen bir analizcisin. Konuşma geçmişini incele ve ilk soruyu ('tüm detaylar') bul."

log_header "Zorlu Simülasyon Tamamlandı."
rm "$HISTORY_FILE"
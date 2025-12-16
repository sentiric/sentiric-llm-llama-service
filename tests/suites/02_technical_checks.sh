#!/bin/bash
source tests/lib/common.sh

log_header "SENARYO: Teknik Yetenek Testleri"

# --- TEST 1: System Prompt Override (Persona) ---
log_info "Test: System Prompt Override (Korsan Modu)"
PAYLOAD='{
    "messages": [{"role": "user", "content": "Selam!"}],
    "system_prompt": "Sen bir korsansın. Her cümlene ARRR diye başla.",
    "max_tokens": 50
}'
RES=$(send_chat "$PAYLOAD" | jq -r '.choices[0].message.content')
if echo "$RES" | grep -iq "ARRR"; then
    log_pass "Korsan modu aktif: $RES"
else
    log_fail "System prompt override çalışmadı: $RES"
fi

# --- TEST 2: JSON Mode (Prompt Engineering ile) ---
log_info "Test: JSON Mode (Prompt ile Zorlama)"
# [FIX] response_format kaldırıldı, istek system_prompt'a eklendi
PAYLOAD='{
    "messages": [
        {"role": "system", "content": "Cevabını SADECE geçerli bir JSON objesi olarak ver. Başka hiçbir şey ekleme."},
        {"role": "user", "content": "Rastgele bir renk ver. Şema: {color: string, hex: string}"}
    ],
    "temperature": 0.0
}'
RES=$(send_chat "$PAYLOAD" | jq -r '.choices[0].message.content')

# Markdown ve tırnak temizliği
CLEAN_JSON=$(echo "$RES" | sed 's/```json//g; s/```//g' | tr "'" '"' | tr -d '\n')
echo -e "🤖 AI (Raw): $RES"
echo -e "🧹 Clean: $CLEAN_JSON"

if echo "$CLEAN_JSON" | jq -e '. | has("color") and has("hex")' >/dev/null 2>&1; then
    log_pass "Geçerli JSON şeması üretildi: $CLEAN_JSON"
else
    log_fail "JSON şeması eksik veya bozuk: $CLEAN_JSON"
fi

# --- TEST 3: LoRA Adapter Switching ---
log_info "Test: LoRA Adapter (Hukukçu Modu)"
PAYLOAD='{
    "messages": [{"role": "user", "content": "Bir sözleşme maddesi önerir misin?"}],
    "lora_adapter": "legal_expert",
    "system_prompt": "Sen bir hukuk danışmanısın.",
    "max_tokens": 100
}'
RES=$(send_chat "$PAYLOAD" | jq -r '.choices[0].message.content')
if [ -n "$RES" ]; then
    log_pass "LoRA adaptörlü istek başarıyla yanıtlandı (Cevap alındı)."
else
    log_fail "LoRA adaptörlü istek başarısız oldu veya boş yanıt döndü."
fi
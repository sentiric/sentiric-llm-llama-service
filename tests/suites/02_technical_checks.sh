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

# --- TEST 2: JSON Mode ---
log_info "Test: JSON Mode (Structured Output)"
PAYLOAD='{
    "messages": [{"role": "user", "content": "Rastgele bir renk ver. JSON formatında: {color: string, hex: string}"}],
    "response_format": {"type": "json_object"},
    "temperature": 0.1
}'
RES=$(send_chat "$PAYLOAD" | jq -r '.choices[0].message.content')

# JSON Validasyonu
if echo "$RES" | jq . >/dev/null 2>&1; then
    if echo "$RES" | grep -q "hex"; then
        log_pass "Geçerli JSON üretildi: $RES"
    else
        log_fail "JSON şeması eksik: $RES"
    fi
else
    log_fail "Bozuk JSON: $RES"
fi
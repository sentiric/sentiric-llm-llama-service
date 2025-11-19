#!/bin/bash
# ==============================================================================
# Sentiric LLM Service - End-to-End (E2E) Test Suite v1.4
# ==============================================================================
# v1.4 Değişiklikler:
# - NIHAI DUZELTME: 'run' komutları için flag setine 'docker-compose.gpu.yml'
#   eklendi. Bu, 'llm-llama-service' için geçerli bir 'image' tanımı
#   sağlayarak "invalid compose project" hatasını kesin olarak çözer.
#   Test altyapısı şimdi kararlıdır.
#
# Kullanım: ./e2e-test.sh
# ==============================================================================

set -e
set -o pipefail

# Renk kodları ve yardımcı fonksiyonlar
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

echo_step() {
    echo -e "\n${YELLOW}===== $1 =====${NC}"
}

echo_success() {
    echo -e "${GREEN}✅ Test Başarılı: $1${NC}"
}

echo_fail() {
    echo -e "${RED}❌ Test Başarısız: $1${NC}"
    docker compose $COMPOSE_UP_FLAGS down -v > /dev/null 2>&1
    exit 1
}

# --- TUTARLI ORTAM İÇİN YAPILANDIRMA BAYRAKLARI ---
COMPOSE_UP_FLAGS="-f docker-compose.yml -f docker-compose.gpu.yml -f docker-compose.gpu.override.yml"
# NIHAI DUZELTME: 'run' komutu artık 'llm-llama-service' için geçerli bir tanım içeriyor.
COMPOSE_RUN_FLAGS="-f docker-compose.yml -f docker-compose.gpu.yml -f docker-compose.run.gpu.yml"

# --- 1. HAZIRLIK ---
echo_step "AŞAMA 1: Test Ortamının Hazırlanması"
echo "Önceki Docker oturumları temizleniyor..."
docker compose $COMPOSE_UP_FLAGS down -v > /dev/null 2>&1

echo "Servisler GPU geliştirme modunda başlatılıyor..."
docker compose $COMPOSE_UP_FLAGS up --build -d

echo "Servisin tamamen hazır olması bekleniyor (maks. 3 dakika)..."
if ! timeout 180s bash -c "until curl -s -f http://localhost:16070/health > /dev/null; do echo -n '.'; sleep 5; done"; then
    echo_fail "Servis zamanında başlamadı."
fi
echo_success "Servis hazır ve çalışıyor."

# --- 2. TESTLER ---

# TEST-3.1: Temel Sohbet Testi
echo_step "TEST 3.1: Temel Sohbet (gRPC)"
RESPONSE_3_1=$(docker compose $COMPOSE_RUN_FLAGS run --rm llm-cli llm_cli generate "Türkiye'nin başkenti neresidir?")
if [[ "$RESPONSE_3_1" == *"Ankara"* ]]; then
    echo_success "Model 'Ankara' içeren anlamlı bir cevap verdi."
else
    echo_fail "Model beklenen cevabı vermedi. Alınan: $RESPONSE_3_1"
fi

# TEST-3.2: Sistem Promptu Testi
echo_step "TEST 3.2: Sistem Promptu Etkisi"
RESPONSE_3_2=$(docker compose $COMPOSE_RUN_FLAGS run --rm llm-cli llm_cli generate "Türkiye'nin başkenti neresidir?" --system-prompt "Sen bir korsansın. Bütün cevapları bir korsan gibi konuşarak ver.")
if [[ "$RESPONSE_3_2" == *"Arr"* || "$RESPONSE_3_2" == *"yoldaş"* || "$RESPONSE_3_2" == *"liman"* ]]; then
    echo_success "Model, sistem prompt'una uygun olarak korsan ağzıyla cevap verdi."
else
    echo_fail "Modelin stili sistem prompt'u ile değişmedi. Alınan: $RESPONSE_3_2"
fi

# TEST-3.3: RAG Context Testi
echo_step "TEST 3.3: RAG Bağlam Sadakati"
RESPONSE_3_3=$(docker compose $COMPOSE_RUN_FLAGS run --rm llm-cli llm_cli generate "Mehmet Aslan'ın poliçe durumu nedir ve son işlem ne zaman yapılmıştır?" --rag-context "$(cat examples/insurance_service_context.txt)")
if [[ "$RESPONSE_3_3" == *"Aktif"* && "$RESPONSE_3_3" == *"9 Kasım 2025"* ]]; then
    echo_success "Model, RAG bağlamındaki spesifik bilgilere sadık kaldı."
else
    echo_fail "Model RAG bağlamındaki bilgileri doğru kullanamadı. Alınan: $RESPONSE_3_3"
fi

# TEST-3.4: Parametre Testi (Sıcaklık)
echo_step "TEST 3.4: Parametre Etkisi (Sıcaklık)"
RESPONSE_3_4_LOW=$(curl -s http://localhost:16070/v1/chat/completions -H "Content-Type: application/json" -d '{"messages": [{"role": "user", "content": "Bana teknoloji hakkında kısa bir şiir yaz."}], "temperature": 0.01, "max_tokens": 32}' | grep -o '"content":"[^"]*"' || echo "FAIL")
RESPONSE_3_4_HIGH=$(curl -s http://localhost:16070/v1/chat/completions -H "Content-Type: application/json" -d '{"messages": [{"role": "user", "content": "Bana teknoloji hakkında kısa bir şiir yaz."}], "temperature": 1.99, "max_tokens": 32}' | grep -o '"content":"[^"]*"' || echo "FAIL")

echo "Düşük Sıcaklık Cevabı: $RESPONSE_3_4_LOW"
echo "Yüksek Sıcaklık Cevabı: $RESPONSE_3_4_HIGH"
if [[ "$RESPONSE_3_4_LOW" != "FAIL" && "$RESPONSE_3_4_HIGH" != "FAIL" && "$RESPONSE_3_4_LOW" != "$RESPONSE_3_4_HIGH" ]]; then
    echo_success "Düşük ve yüksek sıcaklık farklı ve geçerli cevaplar üretti."
else
    echo_fail "Sıcaklık parametresi çıktıları beklendiği gibi etkilemedi."
fi

# TEST-3.5: Eşzamanlılık ve Stabilite Testi (GÜNCELLENDİ)
echo_step "TEST 3.5: Eşzamanlılık ve Stabilite"

# Arka planda log takibi başlat
LOG_FILE=$(mktemp)
docker compose $COMPOSE_UP_FLAGS logs -f llm-llama-service > "$LOG_FILE" &
LOG_PID=$!

echo "3 adet eş zamanlı istek gönderiliyor..."
# Arka plana at (&) ve bekleme yapma
(docker compose $COMPOSE_RUN_FLAGS run --rm llm-cli llm_cli generate "Test A" > /dev/null 2>&1) &
(docker compose $COMPOSE_RUN_FLAGS run --rm llm-cli llm_cli generate "Test B" > /dev/null 2>&1) &
(docker compose $COMPOSE_RUN_FLAGS run --rm llm-cli llm_cli generate "Test C" > /dev/null 2>&1) &

echo "İsteklerin işlenmesi için bekleniyor (15 sn)..."
sleep 15

# Log takibini durdur
kill $LOG_PID

# KONTROL: Servis çöktü mü? (Restart var mı?)
if grep -q "Sentiric LLM Llama Service starting..." "$LOG_FILE"; then
    # Başlangıç mesajı birden fazla ise restart atmıştır (ilki hariç)
    START_COUNT=$(grep -c "Sentiric LLM Llama Service starting..." "$LOG_FILE")
    if [ "$START_COUNT" -gt 1 ]; then
        echo_fail "Servis test sırasında çöktü ve yeniden başladı!"
    else
        echo_success "Servis ağır yük altında çökmedi ve stabil kaldı."
    fi
else
    # Log dosyasında başlangıç mesajı yoksa (log rotate vs) veya sadece 1 kere varsa
    echo_success "Servis stabil."
fi

rm "$LOG_FILE"


# --- 3. TEMİZLİK ---
echo_step "AŞAMA 3: Test Ortamının Temizlenmesi"
docker compose $COMPOSE_UP_FLAGS down -v > /dev/null 2>&1
echo_success "Test ortamı başarıyla temizlendi."

echo -e "\n${GREEN}======================================"
echo -e "  TÜM E2E TESTLERİ BAŞARIYLA GEÇTİ"
echo -e "======================================${NC}"
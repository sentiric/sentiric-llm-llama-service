#!/bin/bash
# ==============================================================================
# Sentiric LLM Service - Long Context & Memory Test v2.1 (Fix: Shebang)
# ==============================================================================

set -e

# Renkler
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

API_URL="http://localhost:16070/v1/chat/completions"
HEALTH_URL="http://localhost:16070/health"
HISTORY_FILE="/tmp/llm_history.json"

echo_step() { echo -e "\n${BLUE}👉 $1${NC}"; }

# 0. Ön Kontroller
echo_step "Hazırlıklar Kontrol Ediliyor..."

if ! command -v jq &> /dev/null; then
    echo -e "${RED}HATA: 'jq' yüklü değil.${NC} Lütfen yükleyin: sudo apt install jq"
    exit 1
fi

if ! command -v curl &> /dev/null; then
    echo -e "${RED}HATA: 'curl' yüklü değil.${NC}"
    exit 1
fi

# 1. Servis Sağlık Kontrolü (Health Check Waiter)
echo_step "Servisin Hazır Olması Bekleniyor..."
echo "Model yüklenirken lütfen bekleyin (Max 180sn)..."

MAX_RETRIES=36 # 36 * 5sn = 180sn
COUNT=0
READY=false

while [ $COUNT -lt $MAX_RETRIES ]; do
    if STATUS=$(curl -s -m 2 "$HEALTH_URL"); then
        IS_HEALTHY=$(echo "$STATUS" | jq -r '.status == "healthy" and .model_ready == true')
        if [ "$IS_HEALTHY" == "true" ]; then
            READY=true
            echo -e "\n${GREEN}✅ Servis ve Model Hazır!${NC}"
            break
        fi
    fi
    echo -n "."
    sleep 5
    ((COUNT++))
done

if [ "$READY" = false ]; then
    echo -e "\n${RED}❌ HATA: Servis belirtilen sürede hazır olamadı.${NC}"
    echo "Lütfen 'make logs' komutu ile modelin indirilip indirilmediğini kontrol edin."
    exit 1
fi

# 2. Başlangıç Temizliği
echo "[]" > "$HISTORY_FILE"

# Simüle edilecek Kullanıcı Senaryosu
declare -a USER_MESSAGES=(
    "Merhaba, internetimle ilgili bir sorun yaşıyorum. Adım Can Yılmaz."
    "Modemimin üzerinde sadece güç ışığı yanıyor, diğerleri sönük."
    "Evet denedim, kapatıp açtım ama değişen bir şey olmadı."
    "Yaklaşık 2 saattir böyle. Müşteri numaram: 887766."
    "Hayır, herhangi bir çalışma olduğu bilgisi gelmedi bana."
    "Kabloları kontrol ettim, hepsi takılı görünüyor."
    "Reset düğmesine basılı tuttum, ışıklar yanıp söndü ama yine aynı."
    "Tamam bekliyorum, hattımı kontrol edin."
    "Peki arıza kaydı oluşturacak mısınız?"
    "Bu arada size en başta ismimi söylemiştim, hatırlıyor musunuz teyit için?"
    "Teşekkürler. Peki tahmini ne zaman düzelir?"
    "Hafta sonu da ekipler çalışıyor mu?"
    "Anladım, peki bu kesinti faturama yansıyacak mı?"
    "Müşteri numaramı tekrar edeyim mi, yoksa kaydettiniz mi?"
    "Tamamdır, iyi çalışmalar dilerim."
)

# Döngü
TURN=0
for MSG in "${USER_MESSAGES[@]}"; do
    ((TURN++))
    echo_step "TUR $TURN: Kullanıcı Konuşuyor..."
    echo -e "${YELLOW}User: $MSG${NC}"

    # History güncelle
    jq --arg content "$MSG" '. += [{"role": "user", "content": $content}]' "$HISTORY_FILE" > "$HISTORY_FILE.tmp" && mv "$HISTORY_FILE.tmp" "$HISTORY_FILE"

    # Payload
    PAYLOAD=$(jq -n \
        --arg sys "Sen yardımsever bir teknik destek uzmanısın. Müşterinin internet sorununu çözmeye çalış. Kısa ve profesyonel cevaplar ver." \
        --slurpfile hist "$HISTORY_FILE" \
        '{
            model: "qwen25_3b_phone_assistant",
            messages: ([{"role": "system", "content": $sys}] + $hist[0]),
            temperature: 0.2,
            max_tokens: 200
        }')

    # İstek
    START_TIME=$(date +%s%3N)
    RESPONSE=$(curl -s -X POST "$API_URL" -H "Content-Type: application/json" -d "$PAYLOAD")
    END_TIME=$(date +%s%3N)
    DURATION=$((END_TIME - START_TIME))

    # Hata Kontrolü
    if [ -z "$RESPONSE" ] || echo "$RESPONSE" | grep -q "error"; then
        echo -e "${RED}API HATASI:${NC} $RESPONSE"
        exit 1
    fi

    # Yanıtı Ayrıştır
    ASSISTANT_REPLY=$(echo "$RESPONSE" | jq -r '.choices[0].message.content')
    TOKENS_TOTAL=$(echo "$RESPONSE" | jq -r '.usage.total_tokens')
    
    echo -e "${GREEN}AI (${DURATION}ms | Total Ctx: ${TOKENS_TOTAL}): ${ASSISTANT_REPLY}${NC}"

    # History güncelle (Asistan)
    jq --arg content "$ASSISTANT_REPLY" '. += [{"role": "assistant", "content": $content}]' "$HISTORY_FILE" > "$HISTORY_FILE.tmp" && mv "$HISTORY_FILE.tmp" "$HISTORY_FILE"

    sleep 0.5
done

echo_step "TEST ANALİZİ"
echo "----------------------------------------------------------------"
LAST_CONVERSATION=$(cat "$HISTORY_FILE")

# Hafıza Kontrolü
if echo "$LAST_CONVERSATION" | grep -i "Can" > /dev/null; then
    echo -e "${GREEN}✅ HAFIZA TESTİ GEÇTİ: Model kullanıcının ismini (Can) hatırladı.${NC}"
else
    echo -e "${RED}❌ HAFIZA TESTİ KALDI: Model ismi hatırlayamadı.${NC}"
fi

if echo "$LAST_CONVERSATION" | grep -i "887766" > /dev/null; then
    echo -e "${GREEN}✅ HAFIZA TESTİ GEÇTİ: Model müşteri numarasını (887766) hatırladı.${NC}"
else
    echo -e "${RED}❌ HAFIZA TESTİ KALDI: Model numarayı hatırlayamadı.${NC}"
fi

echo -e "\nTamamlanan Tur: $TURN"

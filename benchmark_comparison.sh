#!/bin/bash

# Test Parametreleri
PROMPT='{"messages": [{"role": "user", "content": "Bana yapay zeka tarihini özetle."}], "max_tokens": 128, "stream": false}'
URL="http://localhost:16070/v1/chat/completions"

echo "📊 KARŞILAŞTIRMALI PERFORMANS TESTİ"
echo "========================================"

# 1. TEKİL TEST (BASELINE)
echo "1️⃣  Tekil İstek Testi (Referans) Başlıyor..."
start_1=$(date +%s%3N) # Milisaniye
curl -s -X POST $URL -H "Content-Type: application/json" -d "$PROMPT" > /dev/null
end_1=$(date +%s%3N)
duration_1=$((end_1 - start_1))
echo "   ⏱️  Süre: $duration_1 ms"
echo ""

# 2. PARALEL TEST
echo "2️⃣  Paralel İstek Testi (2 Adet) Başlıyor..."
start_2=$(date +%s%3N)
# İki isteği aynı anda arka plana at
curl -s -X POST $URL -H "Content-Type: application/json" -d "$PROMPT" > /dev/null &
PID1=$!
curl -s -X POST $URL -H "Content-Type: application/json" -d "$PROMPT" > /dev/null &
PID2=$!
wait $PID1
wait $PID2
end_2=$(date +%s%3N)
duration_2=$((end_2 - start_2))

echo "   ⏱️  Süre: $duration_2 ms"
echo "========================================"

# 3. SONUÇ ANALİZİ
ratio=$(echo "scale=2; $duration_2 / $duration_1" | bc)

echo "📈 SONUÇ DEĞERLENDİRMESİ:"
echo "   Tekil Süre  : $duration_1 ms"
echo "   Çoklu Süre  : $duration_2 ms"
echo "   Oran (Çoklu/Tekil): ${ratio}x"
echo ""

if (( $(echo "$ratio < 1.5" | bc -l) )); then
    echo "✅ MÜKEMMEL: Sistem PARALEL çalışıyor!"
    echo "   (İki işi yapmak, tek işi yapmaktan çok az daha uzun sürdü.)"
else
    echo "❌ SIRALI: Sistem istekleri kuyruğa alıp tek tek yapıyor."
    echo "   (Süre neredeyse iki katına çıktı.)"
fi
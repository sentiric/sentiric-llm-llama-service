#!/bin/bash
# parallel_verify.sh

# Uzun sürecek bir prompt (Modelin düşünmesi ve yazması zaman almalı)
PROMPT='{"messages": [{"role": "user", "content": "Bana yapay zekanın tarihi hakkında çok uzun ve detaylı, en az 200 kelimelik bir makale yaz."}], "max_tokens": 256, "stream": false}'

echo "🚀 Paralellik Testi Başlıyor..."
start_time=$(date +%s)

# İki isteği AYNI ANDA arka plana (&) atıyoruz
curl -s -X POST http://localhost:16070/v1/chat/completions -H "Content-Type: application/json" -d "$PROMPT" > /dev/null &
PID1=$!
curl -s -X POST http://localhost:16070/v1/chat/completions -H "Content-Type: application/json" -d "$PROMPT" > /dev/null &
PID2=$!

# İkisinin de bitmesini bekle
wait $PID1
wait $PID2

end_time=$(date +%s)
duration=$((end_time - start_time))

echo "----------------------------------------"
echo "⏱️  Toplam Süre: $duration saniye"
echo "----------------------------------------"
echo "ANALİZ:"
echo "Eğer bu süre tek bir isteğin süresine yakınsa (örn: 5-10sn) -> ✅ PARALEL ÇALIŞIYOR"
echo "Eğer bu süre iki katıysa (örn: 20sn+) -> ❌ SIRALI ÇALIŞIYOR"
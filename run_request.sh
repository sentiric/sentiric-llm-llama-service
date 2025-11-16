#!/bin/bash
set -e

# ==============================================================================
# Sentiric LLM Service - Gelişmiş Test ve Örnek Kullanım Script'i (v2.1)
# ==============================================================================
#
# Bu script, llm-llama-service'e zengin bağlam (context) içeren RAG sorguları
# göndermeyi kolaylaştırır. Hem CPU hem de GPU ortamlarını destekler.
#
# Kullanım:
#   ./run_request.sh [seçenekler] "<context_dosyası_yolu>" "<sorgu>"
#
# Seçenekler:
#   -c, --cpu : Testi CPU geliştirme ortamında çalıştırır. (Varsayılan: GPU)
#
# Örnekler için `examples/README.md` dosyasına bakın.
# ==============================================================================

# --- Değişkenleri ve Varsayılanları Ayarla ---
DOCKER_CMD_BASE="docker compose"
# Varsayılan olarak GPU için olan `run.gpu.yml` dosyasını kullan.
DOCKER_CMD_FLAGS="-f docker-compose.run.gpu.yml" 
TARGET_SERVICE="llm-cli"

# --- Komut Satırı Argümanlarını İşle ---
if [[ "$1" == "-c" || "$1" == "--cpu" ]]; then
    # CPU modu seçilirse, hiçbir ek -f bayrağına gerek yok.
    # `docker compose run` komutu, `docker-compose.yml` ve `docker-compose.override.yml`
    # dosyalarını otomatik olarak kullanır.
    DOCKER_CMD_FLAGS=""
    shift # Argümanları sola kaydır
    echo "ℹ️ CPU modu seçildi."
else
    echo "ℹ️ GPU modu varsayılan olarak kullanılıyor. CPU için '-c' bayrağını kullanın."
fi

if [ "$#" -ne 2 ]; then
    echo "❌ HATA: Eksik argüman."
    echo "Kullanım: $0 [-c|--cpu] <context_dosyası_yolu> <sorgu>"
    exit 1
fi

CONTEXT_FILE="$1"
QUERY="$2"

if [ ! -r "$CONTEXT_FILE" ]; then
    echo "❌ HATA: Context dosyası okunamıyor: $CONTEXT_FILE"
    exit 1
fi

CONTEXT_CONTENT=$(cat "$CONTEXT_FILE")

# --- Sistem Prompt'unu Hazırla ---
SYSTEM_PROMPT=$(cat <<'EOF'
Sen, Sentiric platformunda çalışan, yardımsever ve profesyonel bir AI asistansın.
Aşağıdaki 'İlgili Bilgiler' bölümündeki içeriği kullanarak kullanıcının sorusuna doğal, akıcı ve en fazla 2 cümleyle cevap ver.
Cevap yalnızca verilen bağlama dayanmalı; tahmin, uydurma veya bağlam dışı bilgi yok.
Eğer cevap bağlamda yer almıyorsa, bunu nazik ve doğal bir şekilde belirt.

### İlgili Bilgiler:
{context}

### Kullanıcının Sorusu:
{query}

### Cevap:
EOF
)

# --- Testi Çalıştır ---
echo ""
echo "👤 Kullanıcı Sorusu: ${QUERY}"
echo "----------------------------------------------------"

# Final komutunu birleştir ve çalıştır.
# $DOCKER_CMD_FLAGS değişkeni boş olabilir (CPU durumu için), bu bir sorun teşkil etmez.
$DOCKER_CMD_BASE $DOCKER_CMD_FLAGS run --rm $TARGET_SERVICE \
    llm_cli generate "${QUERY}" \
    --system-prompt "${SYSTEM_PROMPT}" \
    --rag-context "${CONTEXT_CONTENT}" \
    --timeout 120
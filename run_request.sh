#!/bin/bash
set -e

# ==============================================================================
# Sentiric LLM Service - Gelişmiş Test ve Örnek Kullanım Script'i (v3.0)
# ==============================================================================
#
# Bu script, llm-llama-service'e zengin bağlam (context) içeren RAG ve
# konuşma geçmişi sorguları göndermeyi kolaylaştırır.
#
# Kullanım:
#   ./run_request.sh [seçenekler] "<sorgu>"
#
# Seçenekler:
#   -c, --cpu          : Testi CPU geliştirme ortamında çalıştırır. (Varsayılan: GPU)
#   -f, --file <path>  : RAG context'i olarak kullanılacak dosyanın yolu.
#   -h, --history <json> : JSON formatında konuşma geçmişi.
#
# Örnekler:
#   ./run_request.sh -f examples/health_service_context.txt "Hastanın son durumu nedir?"
#   ./run_request.sh --history '[{"role":"user","content":"Başkent neresi?"},{"role":"assistant","content":"Ankara."}]' "Nüfusu ne kadar?"
# ==============================================================================

# --- Değişkenleri ve Varsayılanları Ayarla ---
DOCKER_CMD_BASE="docker compose"
DOCKER_CMD_FLAGS_GPU="-f docker-compose.run.gpu.yml"
DOCKER_CMD_FLAGS_CPU=""
TARGET_SERVICE="llm-cli"
USE_GPU=true
RAG_CONTEXT=""
HISTORY_JSON=""
QUERY=""

# --- Komut Satırı Argümanlarını İşle ---
while [[ "$#" -gt 0 ]]; do
    case $1 in
        -c|--cpu) USE_GPU=false; shift ;;
        -f|--file) RAG_CONTEXT=$(cat "$2"); shift 2 ;;
        -h|--history) HISTORY_JSON="$2"; shift 2 ;;
        *) QUERY="$1"; shift ;;
    esac
done

if [ -z "$QUERY" ]; then
    echo "❌ HATA: Sorgu metni belirtilmedi."
    echo "Kullanım: $0 [-c] [-f FILE] [-h JSON] <sorgu>"
    exit 1
fi

# Ortama göre Docker Compose bayraklarını seç
if [ "$USE_GPU" = true ]; then
    DOCKER_CMD_FLAGS="$DOCKER_CMD_FLAGS_GPU"
    echo "ℹ️ GPU modu kullanılıyor."
else
    DOCKER_CMD_FLAGS="$DOCKER_CMD_FLAGS_CPU"
    echo "ℹ️ CPU modu seçildi."
fi

# --- llm_cli için argümanları oluştur ---
CLI_ARGS="generate \"${QUERY}\""

if [ -n "$RAG_CONTEXT" ]; then
    CLI_ARGS+=" --rag-context \"${RAG_CONTEXT}\""
fi

if [ -n "$HISTORY_JSON" ]; then
    CLI_ARGS+=" --history '${HISTORY_JSON}'"
fi

# --- Testi Çalıştır ---
echo ""
echo "👤 Kullanıcı Sorusu: ${QUERY}"
echo "----------------------------------------------------"

# Final komutunu birleştir ve çalıştır.
# `eval` kullanmak, argümanlardaki tırnak işaretlerini doğru bir şekilde işlemesini sağlar.
eval "$DOCKER_CMD_BASE $DOCKER_CMD_FLAGS run --rm $TARGET_SERVICE llm_cli $CLI_ARGS"

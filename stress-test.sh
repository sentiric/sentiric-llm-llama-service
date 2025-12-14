#!/bin/bash
# ==============================================================================
# Sentiric LLM Service - Stress & Stability Test
# ==============================================================================
# Amaç: Dynamic Batching, Memory Leak ve Concurrency hatalarını yakalamak.
# ==============================================================================

set -e

# Renkler
CYAN='\033[0;36m'
NC='\033[0m'

COMPOSE_CLI="-f docker-compose.yml -f docker-compose.gpu.yml -f docker-compose.run.gpu.yml"

echo -e "${CYAN}🔥 SİSTEM STRES TESTİ BAŞLIYOR...${NC}"

# 1. Isınma Turu
echo -e "\n${CYAN}[1/3] Isınma (Warm-up)...${NC}"
docker compose $COMPOSE_CLI run --rm llm-cli llm_cli benchmark --iterations 5

# 2. Concurrency Testi (Orta Yük)
# 4 Eşzamanlı bağlantı, her biri 5 istek. Toplam 20 istek.
echo -e "\n${CYAN}[2/3] Concurrency Testi (4 Parallel Users)...${NC}"
docker compose $COMPOSE_CLI run --rm llm-cli llm_cli benchmark --concurrent 4 --requests 5

# 3. Burst Testi (Ani Yüklenme)
# Dynamic Batching'in sınırlarını zorlamak için.
echo -e "\n${CYAN}[3/3] Burst Testi (High Load)...${NC}"
echo "Sistemin çöküp çökmediğini izleyin..."

# Arka planda logları izlemek isterseniz: docker compose logs -f llm-llama-service &

docker compose $COMPOSE_CLI run --rm llm-cli llm_cli benchmark --concurrent 8 --requests 2

echo -e "\n${CYAN}✅ Stres testi tamamlandı. Logları kontrol edin (OOM veya Crash var mı?).${NC}"
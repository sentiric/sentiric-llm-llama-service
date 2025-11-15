#!/bin/bash

# ----------------------------------------------------
#  External service → LLM request wrapper
#  Usage:
#      ./run_request.sh "<context>" "<query>"
# ----------------------------------------------------

CONTEXT="$1"
QUERY="$2"

PROMPT=$(cat <<EOF
Aşağıdaki 'İlgili Bilgiler' bölümündeki içeriği kullanarak kullanıcının sorusuna
doğal, insancıl, akıcı ve en fazla 2 cümleyle cevap ver.

Cevap yalnızca verilen bağlama dayanmalı; tahmin, uydurma veya bağlam dışı bilgi yok.
Eğer cevap bağlamda yer almıyorsa, bunu nazik ve doğal bir şekilde belirt;
ancak her seferinde aynı ifadeyi kullanma.

### İlgili Bilgiler:
${CONTEXT}

### Kullanıcının Sorusu:
${QUERY}

### Cevap:
EOF
)

docker compose -f docker-compose.run.gpu.yml run --rm llm-cli \
    llm_cli generate "$PROMPT" --timeout 120

# ./run_request.sh "Müşteri dün kargo bekliyordu." "Kargom nerede?"

# ----------------------------------------------------
# Dİkey Hizmetler İçin Örnek Bağlamlar
# ----------------------------------------------------

# vertical-hospitality-service:
# ./run_request.sh "$(cat examples/hospitality_service_context.txt)" "Müşteri dün rezervasyon yaptı. Rezervasyon durumu nedir?"

# vertical-health-service:
# ./run_request.sh "$(cat examples/health_service_context.txt)" "Hastanın son test sonuçları nelerdir?"

# vertical-ecommerce-service:
# ./run_request.sh "$(cat examples/ecommerce_service_context.txt)" "Müşterinin son siparişinin durumu nedir?"

# vertical-legal-service:
# ./run_request.sh "$(cat examples/legal_service_context.txt)" "Müvekkilin davayla ilgili son gelişmeler nelerdir?"

# vertical-public-service:  
# ./run_request.sh "$(cat examples/public_service_context.txt)" "Vatandaşın başvurusu hangi aşamada?"

# vertical-finance-service:  
# ./run_request.sh "$(cat examples/finance_service_context.txt)" "Müşterinin son işlem detayları nelerdir?"

# vertical-insurance-service:  
# ./run_request.sh "$(cat examples/insurance_service_context.txt)" "Sigortalının poliçe durumu nedir?"


# ./run_request.sh "$(cat docs/00_PROJECT_PHILOSOPHY.md)" "Analiz."

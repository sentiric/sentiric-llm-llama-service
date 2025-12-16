PHONY: help up down logs build clean setup test
help:
	@echo "🎨 "
	@echo "-------------------------------------------------------"
	@echo "make setup   : .env dosyasını hazırlar ve sertifikaları kontrol eder"
	@echo "make up      : Tüm AI servislerini başlatır (Local Build, GPU)"
	@echo "make prod    : Hazır imajlardan başlatır (No Build)"
	@echo "make down    : Servisleri durdurur"
	@echo "make logs    : Logları izler"
	@echo "make test    : Tam test matrisini çalıştırır (GPU gerektirir)"

setup:
	@if [ ! -f .env ]; then cp .env.example .env; echo "⚠️ .env oluşturuldu."; fi
	
# Geliştirme Modu: GPU için override dosyalarını kullanır (Local Build)
# Test ortamı ile tam tutarlılık sağlandı.
up: setup
	docker compose -f docker-compose.yml -f docker-compose.gpu.yml -f docker-compose.gpu.override.yml up --build -d

# Üretim Simülasyonu: Override dosyasını YOK SAYAR (Hazır İmaj)
prod: setup
	docker compose -f docker-compose.yml -f docker-compose.gpu.yml up -d

down:
	docker compose -f docker-compose.yml -f docker-compose.gpu.yml -f docker-compose.gpu.override.yml down --remove-orphans

logs:
	docker compose logs -f

test:
	@chmod +x tests/matrix_runner.sh tests/suites/*.sh tests/lib/*.sh
	@./tests/matrix_runner.sh
PHONY: help up down logs build clean setup test-long

help:
	@echo "🎨 "
	@echo "-------------------------------------------------------"
	@echo "make setup   : .env dosyasını hazırlar ve sertifikaları kontrol eder"
	@echo "make up      : Tüm AI servislerini başlatır (Local Build)"
	@echo "make prod    : Hazır imajlardan başlatır (No Build)"
	@echo "make down    : Servisleri durdurur"
	@echo "make logs    : Logları izler"

setup:
	@if [ ! -f .env ]; then cp .env.example .env; echo "⚠️ .env oluşturuldu."; fi
	
# Geliştirme Modu: Override dosyasını kullanır (Local Build)
up: setup
	docker compose -f docker-compose.yml -f docker-compose.override.yml up --build -d

# Üretim Simülasyonu: Override dosyasını YOK SAYAR (Hazır İmaj)
prod: setup
	docker compose -f docker-compose.yml up -d

down:
	docker compose -f docker-compose.yml -f docker-compose.override.yml down --remove-orphans

logs:
	docker compose -f docker-compose.yml logs -f

# YENİ
test-real:
	@chmod +x real-world-phone-test.sh
	@./real-world-phone-test.sh
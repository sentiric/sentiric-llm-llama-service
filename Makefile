# Sentiric LLM Service - Makefile

.PHONY: help up-cpu up-gpu down logs clean cli-gpu

help:
	@echo "🧠 Sentiric LLM Service Yönetim Komutları"
	@echo "------------------------------------------"
	@echo "make up-cpu   : Servisi CPU modunda başlatır"
	@echo "make up-gpu   : Servisi GPU modunda başlatır (NVIDIA)"
	@echo "make down     : Servisi durdurur"
	@echo "make logs     : Logları izler"
	@echo "make clean    : Derleme artıklarını temizler"
	@echo "make cli-gpu  : GPU üzerinde CLI'yı çalıştırır"

up-cpu:
	docker compose up --build -d

up-gpu:
	docker compose -f docker-compose.yml -f docker-compose.gpu.yml -f docker-compose.gpu.override.yml up --build -d

down:
	docker compose down --remove-orphans

logs:
	docker compose logs -f llm-llama-service

clean:
	rm -rf build/
	docker compose down -v

cli-gpu:
	docker compose -f docker-compose.run.gpu.yml run --rm llm-cli llm_cli $(ARGS)
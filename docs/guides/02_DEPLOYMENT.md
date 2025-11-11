# 🚀 Deployment Rehberi

## YENİ: Submodule-Free Development Workflow

### Build Process (Artık daha basit)
```bash
# ESKİ: 
git clone --recursive ...
git submodule update --init

# YENİ:
git clone ...
docker compose up --build -d
```

### Dependency Management
- **llama.cpp**: Otomatik Docker build sırasında indirilir
- **Versiyon Kontrolü**: `git checkout 0750a599` ile sabitlenir
- **Bağımlılıklar**: vcpkg ile merkezi yönetim

### Debugging Improvements
- **Daha az moving part**: Submodule sync sorunu yok
- **Better caching**: Docker layer optimization
- **Simpler reproduction**: Tüm bağımlılıklar otomatik

## Production Deployment

### System Requirements
- **OS**: Ubuntu 20.04+ / RHEL 8+
- **Docker**: 20.10+
- **Docker Compose**: 2.0+
- **RAM**: 4GB minimum, 8GB recommended
- **Storage**: 5GB available
- **CPU**: 4 cores+ recommended

### Deployment Steps

1. **System Preparation**
   ```bash
   # Install Docker
   curl -fsSL https://get.docker.com -o get-docker.sh
   sudo sh get-docker.sh
   
   # Install Docker Compose
   sudo apt-get install docker-compose-plugin
   ```

2. **Application Deployment**
   ```bash
   # Clone repository
   git clone https://github.com/sentiric/sentiric-llm-llama-service.git
   cd sentiric-llm-llama-service
   
   # Download model
   ./models/download.sh
   
   # Deploy
   docker compose up -d
   ```

3. **Verification**
   ```bash
   # Check service status
   docker compose ps
   
   # Health check
   curl http://localhost:16060/health
   
   # Test inference
   docker compose exec llm-llama-service \
     grpc_test_client "Test message"
   ```

## Configuration

### Environment Variables
```yaml
# docker-compose.yml
environment:
  - LLM_LOCAL_SERVICE_MODEL_PATH=/models/phi-3-mini.q4.gguf
  - LOG_LEVEL=info
  - LLM_LOCAL_SERVICE_HTTP_PORT=16060
  - LLM_LOCAL_SERVICE_GRPC_PORT=16061
  - LLM_CONTEXT_SIZE=4096
  - LLM_THREADS=4
```

### Resource Limits
```yaml
# docker-compose.yml
deploy:
  resources:
    limits:
      memory: 4G
      cpus: '2.0'
    reservations:
      memory: 2G
      cpus: '1.0'
```

## Monitoring

### Health Checks
```yaml
healthcheck:
  test: ["CMD", "curl", "-f", "http://localhost:16060/health"]
  interval: 30s
  timeout: 10s
  retries: 3
  start_period: 5m
```

### Log Management
```bash
# Log rotation
docker run --log-driver json-file \
  --log-opt max-size=10m \
  --log-opt max-file=3

# Log monitoring
docker logs -f --tail 100 llm-llama-service
```

## Scaling

### Vertical Scaling
- **Memory**: 8GB for larger models
- **CPU**: More cores for parallel processing
- **Storage**: SSD for faster model loading

### Horizontal Scaling
```yaml
# docker-compose.yml
deploy:
  replicas: 1
  # Note: Stateful service, scaling requires careful design
```

## Backup & Recovery

### Model Backup
```bash
# Backup model
cp models/phi-3-mini.q4.gguf /backup/

# Restore model
cp /backup/phi-3-mini.q4.gguf models/
```

### Configuration Backup
```bash
# Backup configuration
tar czf config-backup.tar.gz \
  docker-compose.yml \
  models/download.sh \
  src/config.h
```

## Security Hardening

### Container Security
```yaml
# docker-compose.yml
security_opt:
  - no-new-privileges:true
read_only: true
tmpfs:
  - /tmp
```

### Network Security
```yaml
# Only expose necessary ports
ports:
  - "127.0.0.1:16060:16060"  # Local only
  - "127.0.0.1:16061:16061"  # Local only
```

## Maintenance

### Updates
```bash
# Service update
git pull origin main
docker compose down
docker compose up --build -d

# Model update
rm models/phi-3-mini.q4.gguf
./models/download.sh
docker compose restart
```

### Cleanup
```bash
# Clean Docker
docker system prune -f
docker volume prune -f

# Log cleanup
find /var/lib/docker/containers -name "*.log" -mtime +7 -delete
```

## Troubleshooting

### Common Issues
- **Port conflicts**: Change ports in docker-compose.yml
- **Memory issues**: Increase resource limits
- **Model corruption**: Re-download model

### Recovery Script
```bash
#!/bin/bash
# recovery.sh
docker compose down
docker system prune -f
./models/download.sh
docker compose up -d
echo "Recovery completed"
```
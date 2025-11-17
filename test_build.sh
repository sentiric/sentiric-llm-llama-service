#!/bin/bash
# test_build.sh
set -e

echo "🧪 Sentiric LLM Service - Build Test Script"
echo "==========================================="

# Build test
echo "🔨 Building service..."
docker compose -f docker-compose.yml -f docker-compose.gpu.yml -f docker-compose.gpu.override.yml build

# Basic syntax check
echo "🔍 Checking code syntax..."
find src -name "*.cpp" -exec echo "Checking {}" \; -exec clang-check -analyze {} -- \;

# Run basic tests
echo "🚀 Starting service for testing..."
docker compose -f docker-compose.yml -f docker-compose.gpu.yml -f docker-compose.gpu.override.yml up -d

# Wait for service to be ready
echo "⏳ Waiting for service to be ready..."
timeout 180 bash -c 'until curl -f http://localhost:16070/health 2>/dev/null; do sleep 5; done'

# Test health endpoint
echo "🏥 Testing health endpoint..."
curl -f http://localhost:16070/health

# Test metrics endpoint
echo "📊 Testing metrics endpoint..."
curl -f http://localhost:16072/metrics

# Test CLI
echo "🔧 Testing CLI..."
docker compose -f docker-compose.run.gpu.yml run --rm llm-cli llm_cli health

echo "✅ All tests passed!"
docker compose -f docker-compose.yml -f docker-compose.gpu.yml -f docker-compose.gpu.override.yml down
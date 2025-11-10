#include <iostream>  // BU SATIRI EKLE
#include "benchmark.h"
#include "spdlog/spdlog.h"
#include <fstream>
#include <iomanip>
#include <thread>
#include <future>

namespace sentiric_llm_cli {

Benchmark::Benchmark(const std::string& endpoint)
    : client_(std::make_unique<CLIClient>(endpoint)) {
}

BenchmarkResult Benchmark::run_performance_test(int iterations, const std::string& prompt) {
    spdlog::info("🎯 Performans testi başlıyor ({} iterasyon)...", iterations);
    
    BenchmarkResult result;
    result.total_requests = iterations;
    auto total_start = std::chrono::steady_clock::now();
    
    for (int i = 0; i < iterations; i++) {
        spdlog::info("Test {}/{} çalıştırılıyor...", i + 1, iterations);
        
        auto start_time = std::chrono::steady_clock::now();
        
        bool success = client_->generate_stream(prompt, 
            [](const std::string& token) {
                // Token bazlı callback - benchmark için boş
            });
        
        auto end_time = std::chrono::steady_clock::now();
        double response_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time).count();
        
        result.response_times.push_back(response_time);
        
        if (success) {
            result.successful_requests++;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Küçük bir bekleme
    }
    
    auto total_end = std::chrono::steady_clock::now();
    result.total_duration = std::chrono::duration_cast<std::chrono::seconds>(total_end - total_start);
    
    // İstatistikleri hesapla
    double total_time = 0;
    for (double time : result.response_times) {
        total_time += time;
    }
    result.average_response_time_ms = total_time / iterations;
    
    result.tokens_per_second = (iterations * 50) / (total_time / 1000.0); // Tahmini
    result.error_rate = ((iterations - result.successful_requests) * 100.0) / iterations;
    
    spdlog::info("✅ Performans testi tamamlandı");
    return result;
}

BenchmarkResult Benchmark::run_concurrent_test(int concurrent_connections, int requests_per_connection) {
    spdlog::info("⚡ Eşzamanlı test başlıyor ({} bağlantı, {} istek/bağlantı)...", 
                concurrent_connections, requests_per_connection);
    
    BenchmarkResult result;
    result.total_requests = concurrent_connections * requests_per_connection;
    auto total_start = std::chrono::steady_clock::now();
    
    std::vector<std::future<int>> futures;
    
    for (int i = 0; i < concurrent_connections; i++) {
        futures.push_back(std::async(std::launch::async, [this, i, requests_per_connection]() {
            int success_count = 0;
            auto client = std::make_unique<CLIClient>();
            
            for (int j = 0; j < requests_per_connection; j++) {
                if (client->generate_stream("Concurrent test prompt " + std::to_string(j))) {
                    success_count++;
                }
            }
            
            return success_count;
        }));
    }
    
    // Sonuçları topla
    for (auto& future : futures) {
        result.successful_requests += future.get();
    }
    
    auto total_end = std::chrono::steady_clock::now();
    result.total_duration = std::chrono::duration_cast<std::chrono::seconds>(total_end - total_start);
    
    result.error_rate = ((result.total_requests - result.successful_requests) * 100.0) / result.total_requests;
    result.tokens_per_second = (result.successful_requests * 50) / result.total_duration.count();
    
    spdlog::info("✅ Eşzamanlı test tamamlandı");
    return result;
}

void Benchmark::generate_report(const BenchmarkResult& result, const std::string& filename) {
    std::ostream* output = &std::cout;
    std::ofstream file;
    
    if (!filename.empty()) {
        file.open(filename);
        output = &file;
    }
    
    *output << "📊 PERFORMANS TEST RAPORU\n";
    *output << "========================\n";
    *output << "Toplam İstek: " << result.total_requests << "\n";
    *output << "Başarılı İstek: " << result.successful_requests << "\n";
    *output << "Hata Oranı: " << std::fixed << std::setprecision(2) << result.error_rate << "%\n";
    *output << "Ortalama Yanıt Süresi: " << result.average_response_time_ms << " ms\n";
    *output << "Token/Saniye: " << std::fixed << std::setprecision(2) << result.tokens_per_second << "\n";
    *output << "Toplam Süre: " << result.total_duration.count() << " saniye\n";
    *output << "========================\n";
    
    if (!result.response_times.empty()) {
        *output << "\nYanıt Süreleri (ms):\n";
        for (size_t i = 0; i < result.response_times.size(); i++) {
            *output << "  " << (i + 1) << ": " << result.response_times[i] << " ms\n";
        }
    }
    
    if (file.is_open()) {
        file.close();
        spdlog::info("Rapor dosyaya kaydedildi: {}", filename);
    }
}

} // namespace sentiric_llm_cli
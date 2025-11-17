#pragma once

#include "config.h"
#include "llama.h"
#include "sentiric/llm/v1/local.pb.h"
#include "spdlog/spdlog.h"
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>
#include <functional>
#include <future>

struct BatchedRequest {
    sentiric::llm::v1::LLMLocalServiceGenerateStreamRequest request;
    std::function<void(const std::string&)> on_token_callback;
    std::function<bool()> should_stop_callback;
    std::promise<void> completion_promise;
    int32_t prompt_tokens;
    int32_t completion_tokens;
};

class DynamicBatcher {
public:
    DynamicBatcher(size_t max_batch_size = 8, 
                   std::chrono::milliseconds max_wait_time = std::chrono::milliseconds(10))
        : max_batch_size_(max_batch_size)
        , max_wait_time_(max_wait_time)
        , running_(true)
        , processing_thread_(&DynamicBatcher::processing_loop, this) {
        spdlog::info("DynamicBatcher initialized: max_batch_size={}, max_wait_time={}ms", 
                    max_batch_size, max_wait_time.count());
    }
    
    ~DynamicBatcher() {
        stop();
    }
    
    std::future<void> add_request(
        const sentiric::llm::v1::LLMLocalServiceGenerateStreamRequest& request,
        const std::function<void(const std::string&)>& on_token_callback,
        const std::function<bool()>& should_stop_callback,
        int32_t& prompt_tokens_out,
        int32_t& completion_tokens_out) {
        
        BatchedRequest batched_request;
        batched_request.request = request;
        batched_request.on_token_callback = on_token_callback;
        batched_request.should_stop_callback = should_stop_callback;
        batched_request.prompt_tokens = 0;
        batched_request.completion_tokens = 0;
        
        auto future = batched_request.completion_promise.get_future();
        
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            request_queue_.push(std::move(batched_request));
        }
        
        queue_cv_.notify_one();
        
        // DÜZELTME: unused parameter warning'larını önle
        (void)prompt_tokens_out;
        (void)completion_tokens_out;
        
        return future;
    }
    
    void stop() {
        running_ = false;
        queue_cv_.notify_all();
        if (processing_thread_.joinable()) {
            processing_thread_.join();
        }
    }
    
    // Batch işleme fonksiyonu - LLMEngine tarafından implemente edilecek
    std::function<void(const std::vector<BatchedRequest>&)> batch_processing_callback;

private:
    void processing_loop() {
        while (running_) {
            std::vector<BatchedRequest> batch;
            
            // Batch oluştur
            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                queue_cv_.wait_for(lock, max_wait_time_, [this]() {
                    return !request_queue_.empty() || !running_;
                });
                
                while (!request_queue_.empty() && batch.size() < max_batch_size_) {
                    batch.push_back(std::move(request_queue_.front()));
                    request_queue_.pop();
                }
            }
            
            // Batch'i işle
            if (!batch.empty() && batch_processing_callback) {
                try {
                    batch_processing_callback(batch);
                    
                    // Sonuçları promise'lere set et
                    for (auto& req : batch) {
                        req.completion_promise.set_value();
                    }
                } catch (const std::exception& e) {
                    spdlog::error("Batch processing failed: {}", e.what());
                    for (auto& req : batch) {
                        try {
                            req.completion_promise.set_exception(std::current_exception());
                        } catch (...) {
                            // Promise zaten set edilmiş olabilir
                        }
                    }
                }
            }
        }
    }
    
    size_t max_batch_size_;
    std::chrono::milliseconds max_wait_time_;
    std::atomic<bool> running_;
    
    std::queue<BatchedRequest> request_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::thread processing_thread_;
};
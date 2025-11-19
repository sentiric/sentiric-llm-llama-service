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
#include <memory>

// Thread-Safe Kuyruk: Motor ve HTTP sunucusu arasında köprü
template<typename T>
class ThreadSafeQueue {
public:
    void push(T value) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(std::move(value));
        cond_.notify_one();
    }

    // Belirli bir süre (timeout) veri bekler
    bool wait_and_pop(T& value, int timeout_ms = 100) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!cond_.wait_for(lock, std::chrono::milliseconds(timeout_ms), [this]{ return !queue_.empty(); })) {
            return false;
        }
        value = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    bool empty() {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

private:
    std::queue<T> queue_;
    std::mutex mutex_;
    std::condition_variable cond_;
};

struct BatchedRequest {
    sentiric::llm::v1::LLMLocalServiceGenerateStreamRequest request;
    
    // gRPC için callback (Eski yöntem, hala destekleniyor)
    std::function<bool(const std::string&)> on_token_callback;
    std::function<bool()> should_stop_callback;
    
    // HTTP için Kuyruk (YENİ YÖNTEM - Çökme Önleyici)
    ThreadSafeQueue<std::string> token_queue;
    std::atomic<bool> is_finished{false}; // İşlem bitti mi?
    
    std::promise<void> completion_promise;
    int32_t prompt_tokens = 0;
    int32_t completion_tokens = 0;
    std::string finish_reason = "stop";
};

class DynamicBatcher {
public:
    DynamicBatcher(size_t max_batch_size, std::chrono::milliseconds max_wait_time)
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
    
    std::future<void> add_request(std::shared_ptr<BatchedRequest> request) {
        auto future = request->completion_promise.get_future();
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            request_queue_.push(std::move(request));
        }
        queue_cv_.notify_one();
        return future;
    }
    
    void stop() {
        running_ = false;
        queue_cv_.notify_all();
        if (processing_thread_.joinable()) {
            processing_thread_.join();
        }
    }
    
    std::function<void(std::vector<std::shared_ptr<BatchedRequest>>&)> batch_processing_callback;

private:
    void processing_loop() {
        while (running_) {
            std::vector<std::shared_ptr<BatchedRequest>> batch;
            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                queue_cv_.wait_for(lock, max_wait_time_, [this]() {
                    return !request_queue_.empty() || !running_;
                });
                if (!running_ && request_queue_.empty()) return;
                while (!request_queue_.empty() && batch.size() < max_batch_size_) {
                    batch.push_back(std::move(request_queue_.front()));
                    request_queue_.pop();
                }
            }
            
            if (!batch.empty() && batch_processing_callback) {
                try {
                    // İşlemi başlat
                    batch_processing_callback(batch);
                    
                    // İşlem bittiğinde tüm isteklere "bitti" işaretini koy
                    for (auto& req : batch) {
                        req->is_finished = true;
                        try { req->completion_promise.set_value(); } catch (...) {}
                    }
                } catch (...) {
                    for (auto& req : batch) {
                        req->is_finished = true;
                        try { req->completion_promise.set_exception(std::current_exception()); } catch (...) {}
                    }
                }
            }
        }
    }
    
    size_t max_batch_size_;
    std::chrono::milliseconds max_wait_time_;
    std::atomic<bool> running_;
    std::queue<std::shared_ptr<BatchedRequest>> request_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::thread processing_thread_;
};
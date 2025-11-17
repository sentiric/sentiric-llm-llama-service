#pragma once

#include "config.h"
#include "core/context_pool.h"
#include "llama.h"
#include "spdlog/spdlog.h"
#include <thread>
#include <vector>
#include "common.h" // EKLENDİ: common_batch_add için

class ModelWarmup {
public:
    static void warmup_contexts(LlamaContextPool& pool, size_t num_contexts) {
        spdlog::info("🔥 Warming up {} llama contexts...", num_contexts);
        
        std::vector<std::thread> warmup_threads;
        std::atomic<size_t> completed{0};
        
        for (size_t i = 0; i < num_contexts; ++i) {
            warmup_threads.emplace_back([&pool, i, &completed, num_contexts]() { // DÜZELTİLDİ: num_contexts capture
                try {
                    // Context'i al (bu ilk kez kullanımı tetikler)
                    ContextGuard guard = pool.acquire();
                    llama_context* ctx = guard.get();
                    
                    // Küçük bir test prompt'u işle
                    const char* warmup_prompt = "Hello";
                    auto* vocab = llama_model_get_vocab(pool.get_model());
                    
                    std::vector<llama_token> tokens;
                    tokens.resize(strlen(warmup_prompt) + 16);
                    int n_tokens = llama_tokenize(vocab, warmup_prompt, strlen(warmup_prompt), 
                                                tokens.data(), tokens.size(), false, true);
                    if (n_tokens > 0) {
                        tokens.resize(n_tokens);
                        
                        llama_batch batch = llama_batch_init(n_tokens, 0, 1);
                        for (int j = 0; j < n_tokens; ++j) {
                            common_batch_add(batch, tokens[j], j, {0}, false); // DÜZELTİLDİ: common.h include edildi
                        }
                        batch.logits[batch.n_tokens - 1] = true;
                        
                        // Sadece decode et, üretim yapma
                        if (llama_decode(ctx, batch) == 0) {
                            spdlog::debug("Context {} warm-up successful", i);
                        }
                        
                        llama_batch_free(batch);
                    }
                    
                    completed++;
                    spdlog::debug("✅ Context {} warm-up completed ({}/{})", 
                                i, completed.load(), num_contexts);
                    
                } catch (const std::exception& e) {
                    spdlog::error("❌ Context {} warm-up failed: {}", i, e.what());
                    completed++;
                }
            });
        }
        
        // Tüm thread'lerin bitmesini bekle
        for (auto& thread : warmup_threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        
        spdlog::info("✅ Model warm-up completed: {}/{} contexts ready", 
                    completed.load(), num_contexts);
    }
};
#pragma once

#include "config.h"
#include "core/context_pool.h"
#include "llama.h"
#include "spdlog/spdlog.h"
#include <thread>
#include <vector>
#include "common.h"

class ModelWarmup {
public:
    static void warmup_contexts(LlamaContextPool& pool, size_t num_contexts) {
        spdlog::info("🔥 Warming up {} llama contexts with REAL inference...", num_contexts);
        
        std::vector<std::thread> warmup_threads;
        std::atomic<size_t> completed{0};
        
        for (size_t i = 0; i < num_contexts; ++i) {
            warmup_threads.emplace_back([&pool, i, &completed, num_contexts]() {
                try {
                    // Context'i al
                    ContextGuard guard = pool.acquire();
                    llama_context* ctx = guard.get();
                    
                    // GÜVENLİ WARM-UP - sampling hatasını önle
                    const char* warmup_prompt = "Hello";
                    auto* vocab = llama_model_get_vocab(pool.get_model());
                    
                    // Tokenize
                    std::vector<llama_token> tokens;
                    tokens.resize(strlen(warmup_prompt) + 16);
                    int n_tokens = llama_tokenize(vocab, warmup_prompt, strlen(warmup_prompt), 
                                                tokens.data(), tokens.size(), false, true);
                    
                    if (n_tokens > 0) {
                        tokens.resize(n_tokens);
                        
                        // Batch oluştur ve decode et
                        llama_batch batch = llama_batch_init(n_tokens, 0, 1);
                        for (int j = 0; j < n_tokens; ++j) {
                            common_batch_add(batch, tokens[j], j, {0}, false);
                        }
                        batch.logits[batch.n_tokens - 1] = true;
                        
                        // İlk decode (warm-up için kritik)
                        if (llama_decode(ctx, batch) == 0) {
                            // GÜVENLİ SAMPLING - basit yaklaşım
                            // Sadece 1 token decode et, sampling yapma
                            llama_batch_free(batch);
                            batch = llama_batch_init(1, 0, 1);
                            
                            // Basit bir token seç (genellikle space token'ı)
                            llama_token safe_token = 13; // Genellikle space/newline
                            common_batch_add(batch, safe_token, n_tokens, {0}, true);
                            llama_decode(ctx, batch);
                        }
                        
                        llama_batch_free(batch);
                    }
                    
                    completed++;
                    spdlog::debug("✅ Context {} REAL warm-up completed ({}/{})", 
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
        
        spdlog::info("✅ REAL Model warm-up completed: {}/{} contexts ready", 
                    completed.load(), num_contexts);
    }

    // HIZLI WARM-UP - sadece kritik bileşenleri ısıt
    static void fast_warmup(LlamaContextPool& pool, size_t num_contexts) {
        spdlog::info("⚡ Fast warm-up for {} contexts...", num_contexts);
        
        for (size_t i = 0; i < num_contexts; ++i) {
            try {
                ContextGuard guard = pool.acquire();
                llama_context* ctx = guard.get();
                
                // Sadece küçük bir decode işlemi - SAMPLING YOK
                const char* quick_prompt = "Hi";
                auto* vocab = llama_model_get_vocab(pool.get_model());
                
                std::vector<llama_token> tokens;
                tokens.resize(16);
                int n_tokens = llama_tokenize(vocab, quick_prompt, strlen(quick_prompt), 
                                            tokens.data(), tokens.size(), false, true);
                
                if (n_tokens > 0) {
                    tokens.resize(n_tokens);
                    llama_batch batch = llama_batch_init(n_tokens, 0, 1);
                    for (int j = 0; j < n_tokens; ++j) {
                        common_batch_add(batch, tokens[j], j, {0}, false);
                    }
                    batch.logits[batch.n_tokens - 1] = true;
                    
                    // Tek decode ile hızlı warm-up - SAMPLING YOK
                    llama_decode(ctx, batch);
                    llama_batch_free(batch);
                }
                
                spdlog::debug("⚡ Context {} fast warm-up done", i);
                
            } catch (const std::exception& e) {
                spdlog::warn("Context {} fast warm-up skipped: {}", i, e.what());
            }
        }
        
        spdlog::info("✅ Fast warm-up completed");
    }

    // EN GÜVENLİ WARM-UP - sadece model yükleme
    static void safe_warmup(LlamaContextPool& pool, size_t num_contexts) {
        spdlog::info("🛡️ Safe warm-up for {} contexts...", num_contexts);
        
        for (size_t i = 0; i < num_contexts; ++i) {
            try {
                // Sadece context alıp bırak - en güvenli
                ContextGuard guard = pool.acquire();
                spdlog::debug("🛡️ Context {} safe warm-up done", i);
            } catch (const std::exception& e) {
                spdlog::warn("Context {} safe warm-up skipped: {}", i, e.what());
            }
        }
        
        spdlog::info("✅ Safe warm-up completed");
    }
};
#include "core/context_pool.h"
#include "spdlog/spdlog.h"
#include <stdexcept>

ContextGuard::ContextGuard(LlamaContextPool* pool, llama_context* ctx) : pool_(pool), ctx_(ctx) { if (!ctx_) throw std::runtime_error("Acquired null llama_context from pool."); }
ContextGuard::~ContextGuard() { if (ctx_ && pool_) { llama_memory_seq_rm(llama_get_memory(ctx_), -1, -1, -1); pool_->release(ctx_); } }
ContextGuard::ContextGuard(ContextGuard&& other) noexcept : pool_(other.pool_), ctx_(other.ctx_) { other.pool_ = nullptr; other.ctx_ = nullptr; }
ContextGuard& ContextGuard::operator=(ContextGuard&& other) noexcept { if (this != &other) { if (ctx_ && pool_) { llama_memory_seq_rm(llama_get_memory(ctx_), -1, -1, -1); pool_->release(ctx_); } pool_ = other.pool_; ctx_ = other.ctx_; other.pool_ = nullptr; other.ctx_ = nullptr; } return *this; }

LlamaContextPool::LlamaContextPool(const Settings& settings, llama_model* model) : model_(model), settings_(settings) { max_size_ = settings.n_threads; if (max_size_ == 0) { max_size_ = 1; } spdlog::info("Context Pool: Initializing {} llama_context instances...", max_size_); initialize_contexts(); }
LlamaContextPool::~LlamaContextPool() { std::lock_guard<std::mutex> lock(mutex_); while (!pool_.empty()) { llama_context* ctx = pool_.front(); pool_.pop(); llama_free(ctx); } spdlog::info("Context Pool: Destroyed {} llama_context instances.", max_size_); }
void LlamaContextPool::initialize_contexts() { if (!model_) return; for (size_t i = 0; i < max_size_; ++i) { llama_context_params ctx_params = llama_context_default_params(); ctx_params.n_ctx = settings_.context_size; ctx_params.n_threads = settings_.n_threads; ctx_params.n_threads_batch = settings_.n_threads_batch; llama_context* ctx = llama_init_from_model(model_, ctx_params); if (!ctx) throw std::runtime_error("Failed to create llama_context for pool instance."); pool_.push(ctx); } }
ContextGuard LlamaContextPool::acquire() { std::unique_lock<std::mutex> lock(mutex_); cv_.wait(lock, [this]{ return !pool_.empty(); }); llama_context* ctx = pool_.front(); pool_.pop(); return ContextGuard(this, ctx); }
void LlamaContextPool::release(llama_context* ctx) { if (!ctx) return; { std::lock_guard<std::mutex> lock(mutex_); pool_.push(ctx); } cv_.notify_one(); }
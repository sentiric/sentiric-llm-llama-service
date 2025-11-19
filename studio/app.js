const app = Vue.createApp({
    data() {
        return {
            history: [],
            userInput: '',
            isLoading: false,
            systemPrompt: 'You are a helpful and professional AI assistant. You must answer in Turkish.',
            ragContext: '',
            availableContexts: [],
            selectedContext: '',
            params: {
                temperature: 0.7,
                max_tokens: 1024
            },
            metrics: { 
                ttft: 0, 
                tps: 0, 
                promptTokens: 0, 
                completionTokens: 0 
            },
            stopController: null
        }
    },
    mounted() {
        this.fetchContexts();
        this.history.push({role: 'assistant', content: 'Merhaba! Ben Sentiric LLM. Geliştirme Stüdyosuna hoş geldiniz.'});
        this.$nextTick(() => { this.$refs.userInput.focus(); });
    },
    methods: {
        renderAssistantMessage(msg) {
            const unsafeHTML = marked.parse(msg.content || '');
            this.$nextTick(() => {
                this.$refs.chatHistory.querySelectorAll('pre code:not([data-highlighted])').forEach((block) => {
                    hljs.highlightElement(block);
                    block.dataset.highlighted = 'true';
                });
            });
            return unsafeHTML;
        },
        async fetchContexts() { 
            try { 
                const r = await fetch('/contexts'); 
                if (r.ok) this.availableContexts = await r.json();
            } catch (e) { 
                console.error("RAG contexts could not be loaded:", e); 
            } 
        },
        async loadContext() { 
            if (!this.selectedContext) { this.ragContext = ''; return; } 
            try { 
                const r = await fetch(`/context/${this.selectedContext}`); 
                if (r.ok) this.ragContext = await r.text();
            } catch (e) { 
                console.error("Selected context could not be loaded:", e); 
            } 
        },
        stopGeneration() { 
            if (this.stopController) {
                this.stopController.abort();
                this.isLoading = false;
            }
        },
        async sendMessage() {
            if (!this.userInput.trim() || this.isLoading) return;
            
            const currentInput = this.userInput;
            this.history.push({ role: 'user', content: currentInput });
            this.userInput = ''; 
            this.isLoading = true;
            this.metrics = { ttft: 0, tps: 0, promptTokens: 0, completionTokens: 0 };
            
            this.stopController = new AbortController();
            const assistantMessage = { role: 'assistant', content: '' };
            this.history.push(assistantMessage);

            this.$nextTick(() => { this.$refs.chatHistory.scrollTop = this.$refs.chatHistory.scrollHeight; });

            try {
                const startTime = performance.now();
                let firstChunkTime = null;

                // OpenAI formatına uygun mesaj listesi oluştur
                const messages = [];
                if (this.systemPrompt.trim()) {
                    messages.push({ role: "system", content: this.systemPrompt });
                }
                
                // RAG context'ini son kullanıcı mesajına ekle
                let finalUserContent = currentInput;
                if (this.ragContext.trim()) {
                    finalUserContent = `Verilen bilgileri kullanarak cevap ver:\n---BAĞLAM---\n${this.ragContext}\n---BAĞLAM SONU---\n\nSoru: ${currentInput}`;
                }
                
                // Önceki geçmişi ve son mesajı ekle
                this.history.slice(0, -2).forEach(h => messages.push(h));
                messages.push({ role: "user", content: finalUserContent });

                const response = await fetch('/v1/chat/completions', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ 
                        model: "sentiric/llm-llama-service",
                        messages: messages,
                        temperature: this.params.temperature,
                        max_tokens: this.params.max_tokens,
                        stream: true 
                    }),
                    signal: this.stopController.signal
                });
                
                if (!response.ok) throw new Error(`Server Error: ${response.status} ${response.statusText}`);
                
                const reader = response.body.getReader();
                const decoder = new TextDecoder();
                
                while (true) {
                    const { value, done } = await reader.read();
                    if (done) break;

                    if (!firstChunkTime) {
                        firstChunkTime = performance.now();
                        this.metrics.ttft = Math.round(firstChunkTime - startTime);
                    }
                    
                    const chunk = decoder.decode(value);
                    const lines = chunk.split('\n').filter(line => line.trim().startsWith('data:'));
                    
                    for (const line of lines) {
                        const jsonStr = line.replace('data: ', '');
                        if (jsonStr.includes('[DONE]')) {
                            break;
                        }
                        try {
                            const data = JSON.parse(jsonStr);
                            const content = data.choices[0]?.delta?.content;
                            if (content) {
                                assistantMessage.content += content;
                                this.metrics.completionTokens++;
                                this.$nextTick(() => { this.$refs.chatHistory.scrollTop = this.$refs.chatHistory.scrollHeight; });
                            }
                        } catch (e) {
                            console.error('Error parsing stream chunk:', jsonStr);
                        }
                    }
                }

                let elapsedTime = (performance.now() - firstChunkTime) / 1000;
                if (this.metrics.completionTokens > 1 && elapsedTime > 0.001) {
                    this.metrics.tps = (this.metrics.completionTokens - 1) / elapsedTime;
                }

            } catch (e) { 
                if (e.name !== 'AbortError') { 
                    assistantMessage.content += `\n\n--- HATA ---\n${e.message}`; 
                }
            } finally { 
                this.isLoading = false;
                this.stopController = null;
                this.$nextTick(() => { if (this.$refs.userInput) this.$refs.userInput.focus(); });
            }
        },
        clearHistory() {
            this.history = [{ role: 'assistant', content: 'Sohbet geçmişi temizlendi. Yeni bir konuya başlayalım!' }];
            this.metrics = { ttft: 0, tps: 0, promptTokens: 0, completionTokens: 0 };
        }
    }
});
app.mount('#app');
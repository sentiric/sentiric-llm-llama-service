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
                repetition_penalty: 1.1
            },
            metrics: { ttft: 0, tps: 0, promptTokens: 0, completionTokens: 0 },
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
            if (!msg.content && !this.isLoading) return '';
            const unsafeHTML = marked.parse(msg.content);
            this.$nextTick(() => {
                this.$refs.chatHistory.querySelectorAll('pre code').forEach((block) => {
                    if (block.dataset.highlighted) return;
                    hljs.highlightElement(block);
                    block.dataset.highlighted = 'true';
                    const pre = block.parentElement;
                    if (pre.querySelector('.copy-button')) return;
                    const copyButton = document.createElement('button');
                    copyButton.innerText = 'Kopyala';
                    copyButton.className = 'copy-button';
                    copyButton.onclick = () => { navigator.clipboard.writeText(block.innerText); copyButton.innerText = 'Kopyalandı!'; setTimeout(() => copyButton.innerText = 'Kopyala', 2000); };
                    pre.appendChild(copyButton);
                });
            });
            return unsafeHTML;
        },
        async fetchContexts() { try { const r = await fetch('/web/contexts'); this.availableContexts = await r.json(); } catch (e) { console.error("RAG contexts could not be loaded:", e); } },
        async loadContext() { if (!this.selectedContext) { this.ragContext = ''; return; } try { const r = await fetch(`/web/context/${this.selectedContext}`); this.ragContext = await r.text(); } catch (e) { console.error("Selected context could not be loaded:", e); } },
        stopGeneration() { if (this.stopController) this.stopController.abort(); },
        async sendMessage() {
            if (!this.userInput.trim() || this.isLoading) return;
            const currentInput = this.userInput;
            this.history.push({ role: 'user', content: currentInput });
            this.userInput = ''; this.isLoading = true;
            this.metrics = { ttft: 0, tps: 0, promptTokens: 0, completionTokens: 0 };
            
            this.stopController = new AbortController();
            const assistantMessage = { role: 'assistant', content: '' };
            this.history.push(assistantMessage);

            this.$nextTick(() => { this.$refs.chatHistory.scrollTop = this.$refs.chatHistory.scrollHeight; });

            try {
                const startTime = performance.now();
                let firstChunkTime = null;
                let lastChunkTime = null;

                const response = await fetch('/web/generate', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ 
                        user_prompt: currentInput, 
                        system_prompt: this.systemPrompt, 
                        rag_context: this.ragContext, 
                        history: this.history.slice(0, -2), 
                        params: this.params 
                    }),
                    signal: this.stopController.signal
                });
                
                if (!response.ok) throw new Error(`Server Error: ${response.status} ${response.statusText}`);
                
                const reader = response.body.getReader();
                const decoder = new TextDecoder();
                
                let streamDone = false;
                while (!streamDone) {
                    const { value, done } = await reader.read();
                    if (done) {
                        streamDone = true;
                        // done olduğunda metadata gelmemiş olabilir, son chunk'ı işle
                    }
                    
                    lastChunkTime = performance.now();
                    if (!firstChunkTime && value) {
                        firstChunkTime = lastChunkTime;
                        this.metrics.ttft = Math.round(firstChunkTime - startTime);
                    }
                    
                    let chunk = done ? '' : decoder.decode(value, { stream: true });
                    const marker = "<<METADATA>>";
                    const markerPos = chunk.indexOf(marker);

                    if (markerPos !== -1) {
                        assistantMessage.content += chunk.substring(0, markerPos);
                        const metadata = JSON.parse(chunk.substring(markerPos + marker.length));
                        this.metrics.promptTokens = metadata.prompt_tokens;
                        this.metrics.completionTokens = metadata.completion_tokens;
                        streamDone = true;
                    } else {
                        assistantMessage.content += chunk;
                    }
                    this.$nextTick(() => { this.$refs.chatHistory.scrollTop = this.$refs.chatHistory.scrollHeight; });
                }

                // --- SAĞLAMLAŞTIRILMIŞ TPS HESAPLAMASI (Döngü Sonrası) ---
                let elapsedTime = (lastChunkTime - firstChunkTime) / 1000;
                if (this.metrics.completionTokens > 1 && elapsedTime > 0.001) {
                    this.metrics.tps = (this.metrics.completionTokens - 1) / elapsedTime;
                } else if (this.metrics.completionTokens > 0) {
                    const totalTime = (lastChunkTime - startTime) / 1000;
                    this.metrics.tps = totalTime > 0 ? this.metrics.completionTokens / totalTime : 0;
                } else {
                    this.metrics.tps = 0;
                }

            } catch (e) { if (e.name !== 'AbortError') { assistantMessage.content += `\n\n--- HATA ---\n${e.message}`; }
            } finally { 
                this.isLoading = false;
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
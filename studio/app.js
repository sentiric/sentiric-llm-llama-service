// STATE MANAGEMENT
const state = {
    isGenerating: false,
    controller: null,
    startTime: 0,
    tokenCount: 0
};

// DOM ELEMENTS
const els = {
    userInput: document.getElementById('userInput'),
    chatHistory: document.getElementById('chatHistory'),
    sendBtn: document.getElementById('sendBtn'),
    stopBtn: document.getElementById('stopBtn'),
    micBtn: document.getElementById('micBtn'),
    systemPrompt: document.getElementById('systemPrompt'),
    tempInput: document.getElementById('tempInput'),
    tempVal: document.getElementById('tempVal'),
    maxTokensInput: document.getElementById('maxTokensInput'),
    ragInput: document.getElementById('ragInput'),
    statusDot: document.getElementById('statusDot'),
    statusText: document.getElementById('statusText'),
    tpsMetric: document.getElementById('tpsMetric'),
    latencyMetric: document.getElementById('latencyMetric')
};

// INITIALIZATION
document.addEventListener('DOMContentLoaded', () => {
    checkHealth();
    setInterval(checkHealth, 10000); // 10 sn'de bir sağlık kontrolü
    
    // Auto-resize textarea
    els.userInput.addEventListener('input', function() {
        this.style.height = 'auto';
        this.style.height = (this.scrollHeight) + 'px';
    });

    // Slider value update
    els.tempInput.addEventListener('input', (e) => els.tempVal.textContent = e.target.value);

    // Enter key to send
    els.userInput.addEventListener('keydown', (e) => {
        if (e.key === 'Enter' && !e.shiftKey) {
            e.preventDefault();
            sendMessage();
        }
    });

    els.sendBtn.addEventListener('click', sendMessage);
    els.stopBtn.addEventListener('click', stopGeneration);
});

// --- CORE LOGIC ---

async function checkHealth() {
    try {
        const res = await fetch('/health');
        if (res.ok) {
            const data = await res.json();
            if (data.status === 'healthy') {
                updateStatus(true, 'Bağlı & Hazır');
            } else {
                updateStatus(false, 'Model Yükleniyor...');
            }
        } else {
            updateStatus(false, 'Servis Erişilemez');
        }
    } catch (e) {
        updateStatus(false, 'Bağlantı Hatası');
    }
}

function updateStatus(isOnline, text) {
    els.statusDot.className = isOnline ? 'status-indicator connected' : 'status-indicator';
    els.statusText.textContent = text;
    els.sendBtn.disabled = !isOnline;
}

function appendMessage(role, content) {
    const msgDiv = document.createElement('div');
    msgDiv.className = `message ${role}`;
    
    const avatarIcon = role === 'user' ? '<i class="fas fa-user"></i>' : '<i class="fas fa-robot"></i>';
    const roleName = role === 'user' ? 'Siz' : 'Sentiric AI';

    msgDiv.innerHTML = `
        <div class="message-avatar" title="${roleName}">${avatarIcon}</div>
        <div class="message-content">${content}</div>
    `;

    // Empty state varsa kaldır
    const emptyState = document.querySelector('.empty-state');
    if (emptyState) emptyState.remove();

    els.chatHistory.appendChild(msgDiv);
    scrollToBottom();
    return msgDiv.querySelector('.message-content');
}

async function sendMessage() {
    const text = els.userInput.value.trim();
    if (!text || state.isGenerating) return;

    // UI Reset
    els.userInput.value = '';
    els.userInput.style.height = 'auto';
    
    appendMessage('user', escapeHtml(text));
    
    // AI Placeholder
    const aiContentDiv = appendMessage('ai', '<span class="cursor"></span>');
    
    // Prepare Generation
    setGeneratingState(true);
    state.controller = new AbortController();
    state.startTime = Date.now();
    state.tokenCount = 0;
    
    let fullResponse = "";

    const messages = [];
    // 1. System Prompt
    if (els.systemPrompt.value.trim()) {
        messages.push({ role: "system", content: els.systemPrompt.value });
    }
    
    // 2. RAG Context Injection
    const userContent = els.ragInput.value.trim() 
        ? `BAĞLAM BİLGİSİ:\n${els.ragInput.value.trim()}\n\nSORU:\n${text}`
        : text;

    messages.push({ role: "user", content: userContent });

    try {
        const response = await fetch('/v1/chat/completions', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                messages: messages,
                temperature: parseFloat(els.tempInput.value),
                max_tokens: parseInt(els.maxTokensInput.value),
                stream: true
            }),
            signal: state.controller.signal
        });

        if (!response.ok) throw new Error(`API Hatası: ${response.status}`);

        const reader = response.body.getReader();
        const decoder = new TextDecoder();
        let buffer = "";

        while (true) {
            const { done, value } = await reader.read();
            if (done) break;

            buffer += decoder.decode(value, { stream: true });
            const lines = buffer.split('\n');
            buffer = lines.pop(); // Eksik parça varsa sonraki tura sakla

            for (const line of lines) {
                const trimmed = line.trim();
                if (!trimmed || trimmed === 'data: [DONE]') continue;
                if (trimmed.startsWith('data: ')) {
                    try {
                        const json = JSON.parse(trimmed.substring(6));
                        const delta = json.choices[0]?.delta?.content;
                        if (delta) {
                            fullResponse += delta;
                            // Canlı Markdown render (performans için her chunk'ta değil de,
                            // belki requestAnimationFrame ile yapılabilir ama şimdilik direkt yapıyoruz)
                            aiContentDiv.innerHTML = marked.parse(fullResponse) + '<span class="cursor"></span>';
                            state.tokenCount++;
                            scrollToBottom();
                        }
                    } catch (e) { console.warn("JSON Parse error", e); }
                }
            }
        }

        // Final render (cursor'ı kaldır ve highlight yap)
        aiContentDiv.innerHTML = marked.parse(fullResponse);
        hljs.highlightAll();
        updateMetrics(Date.now() - state.startTime, state.tokenCount);

    } catch (err) {
        if (err.name === 'AbortError') {
            aiContentDiv.innerHTML += " <br><em>[İşlem kullanıcı tarafından durduruldu]</em>";
        } else {
            aiContentDiv.innerHTML += `<br><div style="color:#ef4444; margin-top:10px;">HATA: ${err.message}</div>`;
        }
    } finally {
        setGeneratingState(false);
        state.controller = null;
    }
}

function stopGeneration() {
    if (state.controller) {
        state.controller.abort();
    }
}

// --- UTILS ---

function setGeneratingState(isGenerating) {
    state.isGenerating = isGenerating;
    if (isGenerating) {
        els.sendBtn.classList.add('hidden');
        els.stopBtn.classList.remove('hidden');
        els.userInput.disabled = true;
    } else {
        els.sendBtn.classList.remove('hidden');
        els.stopBtn.classList.add('hidden');
        els.userInput.disabled = false;
        els.userInput.focus();
    }
}

function scrollToBottom() {
    const chatArea = els.chatHistory;
    // Sadece kullanıcı en alttaysa otomatik kaydır (UX kuralı)
    // Ancak burada basitlik adına her zaman kaydırıyoruz.
    chatArea.scrollTop = chatArea.scrollHeight;
}

function updateMetrics(durationMs, tokens) {
    const seconds = durationMs / 1000;
    const tps = tokens / seconds;
    
    els.tpsMetric.textContent = tps.toFixed(2);
    els.latencyMetric.textContent = `${durationMs} ms`;
}

function escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

// --- WEB SPEECH API (Microphone) ---
if ('webkitSpeechRecognition' in window || 'SpeechRecognition' in window) {
    const SpeechRecognition = window.SpeechRecognition || window.webkitSpeechRecognition;
    const recognition = new SpeechRecognition();
    recognition.lang = 'tr-TR';
    recognition.continuous = false;
    
    recognition.onstart = () => els.micBtn.style.color = '#ef4444';
    recognition.onend = () => els.micBtn.style.color = '';
    
    recognition.onresult = (event) => {
        const transcript = event.results[0][0].transcript;
        els.userInput.value += (els.userInput.value ? ' ' : '') + transcript;
        els.userInput.style.height = 'auto';
        els.userInput.style.height = els.userInput.scrollHeight + 'px';
        els.userInput.focus();
    };

    els.micBtn.addEventListener('click', () => recognition.start());
} else {
    els.micBtn.style.display = 'none';
}
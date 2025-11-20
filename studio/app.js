// GLOBAL STATE
const state = {
    isGenerating: false,
    controller: null,
    startTime: 0,
    tokenCount: 0,
    theme: 'light' // Varsayılan Light Theme
};

// DOM ELEMENTS
const els = {
    userInput: document.getElementById('userInput'),
    chatMessages: document.getElementById('chatMessages'),
    sendBtn: document.getElementById('sendBtn'),
    stopBtn: document.getElementById('stopBtn'),
    systemPrompt: document.getElementById('systemPrompt'),
    ragInput: document.getElementById('ragInput'),
    tempInput: document.getElementById('tempInput'),
    tempVal: document.getElementById('tempVal'),
    maxTokensInput: document.getElementById('maxTokensInput'),
    statusDot: document.querySelector('.status-dot'),
    statusText: document.getElementById('statusText'),
    tpsMetric: document.getElementById('tpsMetric'),
    latencyMetric: document.getElementById('latencyMetric'),
    tokenMetric: document.getElementById('tokenMetric')
};

// --- INIT ---
document.addEventListener('DOMContentLoaded', () => {
    // 1. Sağlık Kontrolü Başlat
    checkHealth();
    setInterval(checkHealth, 10000);

    // 2. Event Listener'ları Bağla
    setupEventListeners();

    // 3. Temiz Başlangıç: Odak Modu (Paneller Kapalı)
    setView('focus');
});

function setupEventListeners() {
    // Textarea Auto-resize
    if(els.userInput) {
        els.userInput.addEventListener('input', function() {
            this.style.height = 'auto';
            this.style.height = (this.scrollHeight) + 'px';
        });

        // Enter ile Gönderme
        els.userInput.addEventListener('keydown', (e) => {
            if (e.key === 'Enter' && !e.shiftKey) {
                e.preventDefault();
                sendMessage();
            }
        });
    }

    // Butonlar
    if(els.sendBtn) els.sendBtn.addEventListener('click', sendMessage);
    if(els.stopBtn) els.stopBtn.addEventListener('click', stopGeneration);
    
    // Slider Değer Güncelleme
    if(els.tempInput) {
        els.tempInput.addEventListener('input', (e) => {
            if(els.tempVal) els.tempVal.textContent = e.target.value;
        });
    }

    // Mikrofon (Varsa)
    const micBtn = document.getElementById('micBtn');
    if (micBtn) {
        if ('webkitSpeechRecognition' in window) {
            const recognition = new webkitSpeechRecognition();
            recognition.lang = 'tr-TR';
            recognition.onstart = () => micBtn.style.color = '#ef4444';
            recognition.onend = () => micBtn.style.color = '';
            recognition.onresult = (e) => {
                if(els.userInput) els.userInput.value += e.results[0][0].transcript;
            };
            micBtn.addEventListener('click', () => recognition.start());
        } else {
            micBtn.style.display = 'none';
        }
    }
}

// --- CORE LOGIC (API) ---

async function sendMessage() {
    const text = els.userInput.value.trim();
    if (!text || state.isGenerating) return;

    // UI Temizle
    els.userInput.value = '';
    els.userInput.style.height = 'auto';
    
    // Hoşgeldin ekranını kaldır
    const welcome = document.querySelector('.welcome-screen');
    if(welcome) welcome.style.display = 'none';

    appendMessage('user', text);
    
    // AI Mesaj Kutusu Oluştur
    const aiMsgContent = appendMessage('ai', '<span class="cursor"></span>');
    
    setGeneratingState(true);
    state.controller = new AbortController();
    state.startTime = Date.now();
    state.tokenCount = 0;
    
    let fullResponse = "";

    // Prompt Hazırla
    const messages = [];
    if (els.systemPrompt && els.systemPrompt.value.trim()) {
        messages.push({ role: "system", content: els.systemPrompt.value });
    }
    
    // RAG Bağlamı Ekle
    const ragText = els.ragInput ? els.ragInput.value.trim() : "";
    const finalUserContent = ragText 
        ? `BAĞLAM BİLGİSİ:\n${ragText}\n\nKULLANICI SORUSU:\n${text}`
        : text;

    messages.push({ role: "user", content: finalUserContent });

    try {
        const response = await fetch('/v1/chat/completions', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                messages: messages,
                temperature: parseFloat(els.tempInput ? els.tempInput.value : 0.7),
                max_tokens: parseInt(els.maxTokensInput ? els.maxTokensInput.value : 2048),
                stream: true
            }),
            signal: state.controller.signal
        });

        if (!response.ok) throw new Error(`API Error: ${response.status}`);

        const reader = response.body.getReader();
        const decoder = new TextDecoder();
        let buffer = "";

        while (true) {
            const { done, value } = await reader.read();
            if (done) break;

            buffer += decoder.decode(value, { stream: true });
            const lines = buffer.split('\n');
            buffer = lines.pop();

            for (const line of lines) {
                const trimmed = line.trim();
                if (!trimmed || trimmed === 'data: [DONE]') continue;
                if (trimmed.startsWith('data: ')) {
                    try {
                        const json = JSON.parse(trimmed.substring(6));
                        const delta = json.choices[0]?.delta?.content;
                        if (delta) {
                            fullResponse += delta;
                            aiMsgContent.innerHTML = marked.parse(fullResponse) + '<span class="cursor"></span>';
                            state.tokenCount++;
                            scrollToBottom();
                        }
                    } catch (e) {}
                }
            }
        }

        // Bitiş
        aiMsgContent.innerHTML = marked.parse(fullResponse);
        hljs.highlightAll();
        updateMetrics(Date.now() - state.startTime, state.tokenCount);

    } catch (err) {
        if (err.name === 'AbortError') {
            aiMsgContent.innerHTML += " <br><em>[Durduruldu]</em>";
        } else {
            aiMsgContent.innerHTML += `<br><span style="color:#ef4444">HATA: ${err.message}</span>`;
        }
    } finally {
        setGeneratingState(false);
        state.controller = null;
    }
}

function stopGeneration() {
    if (state.controller) state.controller.abort();
}

// --- UI YARDIMCILARI ---

function appendMessage(role, content) {
    const div = document.createElement('div');
    div.className = `message ${role}`;
    
    const avatarIcon = role === 'user' ? '<i class="fas fa-user"></i>' : '<i class="fas fa-robot"></i>';
    
    div.innerHTML = `
        <div class="avatar">${avatarIcon}</div>
        <div class="msg-content">${content}</div>
    `;
    
    els.chatMessages.appendChild(div);
    scrollToBottom();
    return div.querySelector('.msg-content');
}

function setGeneratingState(active) {
    state.isGenerating = active;
    if (active) {
        if(els.sendBtn) els.sendBtn.classList.add('hidden');
        if(els.stopBtn) els.stopBtn.classList.remove('hidden');
    } else {
        if(els.sendBtn) els.sendBtn.classList.remove('hidden');
        if(els.stopBtn) els.stopBtn.classList.add('hidden');
        if(els.userInput) els.userInput.focus();
    }
}

function updateMetrics(duration, tokens) {
    const seconds = duration / 1000;
    const tps = tokens / seconds;
    if(els.tpsMetric) els.tpsMetric.textContent = tps.toFixed(2);
    if(els.latencyMetric) els.latencyMetric.textContent = `${duration} ms`;
    if(els.tokenMetric) els.tokenMetric.textContent = tokens;
}

function scrollToBottom() {
    if(els.chatMessages) els.chatMessages.scrollTop = els.chatMessages.scrollHeight;
}

async function checkHealth() {
    try {
        const res = await fetch('/health');
        const data = await res.json();
        if (data.status === 'healthy') {
            if(els.statusDot) els.statusDot.className = 'status-dot connected';
            if(els.statusText) els.statusText.textContent = 'Hazır';
        } else {
            if(els.statusDot) els.statusDot.className = 'status-dot';
            if(els.statusText) els.statusText.textContent = 'Yükleniyor...';
        }
    } catch (e) {
        if(els.statusDot) els.statusDot.className = 'status-dot';
        if(els.statusText) els.statusText.textContent = 'Bağlantı Yok';
    }
}

// --- PANEL & VIEW LOGIC (Global Functions for HTML onClick) ---

window.togglePanel = function(id) {
    const panel = document.getElementById(id);
    if (!panel) return;
    
    if (panel.style.display === 'none') {
        panel.style.display = 'flex';
    } else {
        panel.style.display = 'none';
    }
};

window.switchTab = function(tabName) {
    // Butonları güncelle
    document.querySelectorAll('.context-tab').forEach(t => t.classList.remove('active'));
    // İçerikleri gizle
    document.querySelectorAll('.tab-content').forEach(c => c.style.display = 'none');
    
    // Aktif yap
    event.currentTarget.classList.add('active');
    document.getElementById(tabName + 'Tab').style.display = 'block';
};

window.setView = function(mode) {
    // Reset Buttons
    document.querySelectorAll('.view-btn').forEach(b => b.classList.remove('active'));
    event.currentTarget.classList.add('active');

    const left = document.getElementById('leftPanel');
    const right = document.getElementById('rightPanel');

    if (mode === 'focus') {
        // Her iki paneli kapat
        if(left) left.style.display = 'none';
        if(right) right.style.display = 'none';
    } else if (mode === 'workbench') {
        // Her iki paneli aç
        if(left) left.style.display = 'flex';
        if(right) right.style.display = 'flex';
    }
};

window.switchTheme = function() {
    const body = document.body;
    if (state.theme === 'dark') {
        body.setAttribute('data-theme', 'light');
        state.theme = 'light';
    } else {
        body.setAttribute('data-theme', 'dark');
        state.theme = 'dark';
    }
};
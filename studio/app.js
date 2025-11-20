// GLOBAL STATE
const state = {
    isGenerating: false,
    controller: null,
    startTime: 0,
    tokenCount: 0,
    theme: 'light'
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
    // 1. Varsayılan Görünüm (Focus Mode)
    // "window.setView" fonksiyonunu doğrudan çağırmak yerine, 
    // parametre ile DOM elemanlarını manuel ayarlıyoruz ki event hatası olmasın.
    setViewInternal('focus');

    // 2. Event Listener'ları Bağla
    setupEventListeners();

    // 3. Sağlık Kontrolü Başlat
    checkHealth();
    setInterval(checkHealth, 10000);
});

function setupEventListeners() {
    if(els.userInput) {
        els.userInput.addEventListener('input', function() {
            this.style.height = 'auto';
            this.style.height = (this.scrollHeight) + 'px';
        });

        els.userInput.addEventListener('keydown', (e) => {
            if (e.key === 'Enter' && !e.shiftKey) {
                e.preventDefault();
                sendMessage();
            }
        });
    }

    if(els.sendBtn) els.sendBtn.addEventListener('click', sendMessage);
    if(els.stopBtn) els.stopBtn.addEventListener('click', stopGeneration);
    
    if(els.tempInput) {
        els.tempInput.addEventListener('input', (e) => {
            if(els.tempVal) els.tempVal.textContent = e.target.value;
        });
    }

    // Web Speech API
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

// --- CORE LOGIC ---

async function sendMessage() {
    const text = els.userInput.value.trim();
    if (!text || state.isGenerating) return;

    els.userInput.value = '';
    els.userInput.style.height = 'auto';
    
    const welcome = document.querySelector('.welcome-screen');
    if(welcome) welcome.style.display = 'none';

    appendMessage('user', text);
    
    // AI "Düşünüyor" Göstergesi
    const loadingId = 'loading-' + Date.now();
    appendLoading(loadingId);

    setGeneratingState(true);
    state.controller = new AbortController();
    state.startTime = Date.now();
    state.tokenCount = 0;
    
    // Prompt Hazırlığı
    const messages = [];
    if (els.systemPrompt && els.systemPrompt.value.trim()) {
        messages.push({ role: "system", content: els.systemPrompt.value });
    }
    
    const ragText = els.ragInput ? els.ragInput.value.trim() : "";
    const finalUserContent = ragText 
        ? `BAĞLAM BİLGİSİ:\n${ragText}\n\nSORU:\n${text}`
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

        // Loading'i kaldır ve gerçek AI balonunu oluştur
        removeLoading(loadingId);
        const aiMsgContent = appendMessage('ai', '<span class="cursor"></span>');

        if (!response.ok) throw new Error(`API Hatası: ${response.status}`);

        const reader = response.body.getReader();
        const decoder = new TextDecoder();
        let buffer = "";
        let fullResponse = "";

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
                            // Basit render (Markdown olmadan hız için)
                            // aiMsgContent.textContent = fullResponse; 
                            
                            // Gelişmiş Render (Her chunk'ta marked çağırmak pahalıdır ama görsel için yapıyoruz)
                            aiMsgContent.innerHTML = marked.parse(fullResponse) + '<span class="cursor"></span>';
                            
                            state.tokenCount++;
                            scrollToBottom();
                        }
                    } catch (e) {}
                }
            }
        }

        // Bitiş Render
        aiMsgContent.innerHTML = marked.parse(fullResponse);
        hljs.highlightAll();
        updateMetrics(Date.now() - state.startTime, state.tokenCount);

    } catch (err) {
        removeLoading(loadingId); // Hata durumunda loading kaldıysa sil
        if (err.name !== 'AbortError') {
             // Eğer AI balonu henüz oluşturulmadıysa oluştur
             const errContent = appendMessage('ai', '');
             errContent.innerHTML = `<span style="color:#ef4444">HATA: ${err.message}</span>`;
        }
    } finally {
        setGeneratingState(false);
        state.controller = null;
    }
}

function stopGeneration() {
    if (state.controller) state.controller.abort();
}

// --- UI HELPERS ---

function appendMessage(role, content) {
    const div = document.createElement('div');
    div.className = `message ${role}`;
    
    const avatar = role === 'user' ? '<i class="fas fa-user"></i>' : '<i class="fas fa-robot"></i>';
    
    div.innerHTML = `
        <div class="avatar">${avatar}</div>
        <div class="msg-content">${content}</div>
    `;
    
    els.chatMessages.appendChild(div);
    scrollToBottom();
    return div.querySelector('.msg-content');
}

function appendLoading(id) {
    const div = document.createElement('div');
    div.className = 'message ai loading-msg';
    div.id = id;
    div.innerHTML = `
        <div class="avatar"><i class="fas fa-robot"></i></div>
        <div class="msg-content" style="background:transparent; box-shadow:none; padding:0;">
            <div class="typing"><span></span><span></span><span></span></div>
        </div>
    `;
    els.chatMessages.appendChild(div);
    scrollToBottom();
}

function removeLoading(id) {
    const el = document.getElementById(id);
    if (el) el.remove();
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
            if(els.statusText) els.statusText.textContent = 'Model Yükleniyor...';
        }
    } catch (e) {
        if(els.statusDot) els.statusDot.className = 'status-dot';
        if(els.statusText) els.statusText.textContent = 'Bağlantı Yok';
    }
}

// --- GLOBAL EXPORTS & VIEW LOGIC ---

window.togglePanel = function(id) {
    const panel = document.getElementById(id);
    if (!panel) return;
    
    if (panel.style.display === 'none') {
        panel.style.display = 'flex';
        // Timeout to allow CSS transition
        setTimeout(() => panel.style.opacity = '1', 10);
    } else {
        panel.style.display = 'none';
        panel.style.opacity = '0';
    }
};

window.switchTab = function(tabName) {
    document.querySelectorAll('.context-tab').forEach(t => t.classList.remove('active'));
    document.querySelectorAll('.tab-content').forEach(c => c.style.display = 'none');
    
    // event.currentTarget hatasını önlemek için, fonksiyonu çağıran elemanı bul
    // Basit çözüm: event global değişkenini kullan (inline onclick için çalışır)
    if (window.event) {
        window.event.currentTarget.classList.add('active');
    }
    document.getElementById(tabName + 'Tab').style.display = 'block';
};

// Internal helper to set view without relying on event object
function setViewInternal(mode) {
    const left = document.getElementById('leftPanel');
    const right = document.getElementById('rightPanel');
    const buttons = document.querySelectorAll('.view-btn');

    // Update buttons visual state
    buttons.forEach(btn => {
        btn.classList.remove('active');
        if (btn.getAttribute('onclick').includes(`'${mode}'`)) {
            btn.classList.add('active');
        }
    });

    if (mode === 'focus') {
        if(left) left.style.display = 'none';
        if(right) right.style.display = 'none';
    } else if (mode === 'workbench') {
        if(left) left.style.display = 'flex';
        if(right) right.style.display = 'flex';
    }
}

// Public function called by buttons
window.setView = function(mode) {
    setViewInternal(mode);
};

window.switchTheme = function() {
    const body = document.body;
    if (state.theme === 'dark') {
        body.removeAttribute('data-theme'); // Light is default
        state.theme = 'light';
    } else {
        body.setAttribute('data-theme', 'dark');
        state.theme = 'dark';
    }
};
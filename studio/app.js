// GLOBAL STATE
const state = {
    isGenerating: false,
    controller: null,
    startTime: 0,
    tokenCount: 0,
    theme: 'dark' // 'dark' or 'light'
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
    checkHealth();
    setInterval(checkHealth, 10000);
    setupEventListeners();
});

function setupEventListeners() {
    // Auto-resize textarea
    els.userInput.addEventListener('input', function() {
        this.style.height = 'auto';
        this.style.height = (this.scrollHeight) + 'px';
    });

    // Send on Enter
    els.userInput.addEventListener('keydown', (e) => {
        if (e.key === 'Enter' && !e.shiftKey) {
            e.preventDefault();
            sendMessage();
        }
    });

    // Buttons
    els.sendBtn.addEventListener('click', sendMessage);
    els.stopBtn.addEventListener('click', stopGeneration);
    
    // Slider Update
    els.tempInput.addEventListener('input', (e) => els.tempVal.textContent = e.target.value);

    // Mic Button (Basic Web Speech API)
    const micBtn = document.getElementById('micBtn');
    if ('webkitSpeechRecognition' in window) {
        const recognition = new webkitSpeechRecognition();
        recognition.lang = 'tr-TR';
        recognition.onstart = () => micBtn.style.color = '#ef4444';
        recognition.onend = () => micBtn.style.color = '';
        recognition.onresult = (e) => {
            els.userInput.value += e.results[0][0].transcript;
        };
        micBtn.addEventListener('click', () => recognition.start());
    } else {
        micBtn.style.display = 'none';
    }
}

// --- CORE LOGIC (API) ---

async function sendMessage() {
    const text = els.userInput.value.trim();
    if (!text || state.isGenerating) return;

    // UI Updates
    els.userInput.value = '';
    els.userInput.style.height = 'auto';
    const welcome = document.querySelector('.welcome-screen');
    if(welcome) welcome.remove();

    appendMessage('user', text);
    
    // Prepare AI Message Container
    const aiMsgContent = appendMessage('ai', '<span class="cursor"></span>');
    
    setGeneratingState(true);
    state.controller = new AbortController();
    state.startTime = Date.now();
    state.tokenCount = 0;
    
    let fullResponse = "";

    // Build Prompt
    const messages = [];
    if (els.systemPrompt.value.trim()) {
        messages.push({ role: "system", content: els.systemPrompt.value });
    }
    
    // Inject RAG if present
    const ragText = els.ragInput.value.trim();
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
                temperature: parseFloat(els.tempInput.value),
                max_tokens: parseInt(els.maxTokensInput.value),
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

        // Finalize
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

function setGeneratingState(active) {
    state.isGenerating = active;
    if (active) {
        els.sendBtn.classList.add('hidden');
        els.stopBtn.classList.remove('hidden');
    } else {
        els.sendBtn.classList.remove('hidden');
        els.stopBtn.classList.add('hidden');
        els.userInput.focus();
    }
}

function updateMetrics(duration, tokens) {
    const seconds = duration / 1000;
    const tps = tokens / seconds;
    els.tpsMetric.textContent = tps.toFixed(2);
    els.latencyMetric.textContent = `${duration} ms`;
    els.tokenMetric.textContent = tokens;
}

function scrollToBottom() {
    els.chatMessages.scrollTop = els.chatMessages.scrollHeight;
}

async function checkHealth() {
    try {
        const res = await fetch('/health');
        const data = await res.json();
        if (data.status === 'healthy') {
            els.statusDot.className = 'status-dot connected';
            els.statusText.textContent = 'Bağlı';
        } else {
            els.statusDot.className = 'status-dot';
            els.statusText.textContent = 'Hazırlanıyor...';
        }
    } catch (e) {
        els.statusDot.className = 'status-dot';
        els.statusText.textContent = 'Bağlantı Yok';
    }
}

// --- PANEL & VIEW LOGIC ---

function togglePanel(id) {
    const panel = document.getElementById(id);
    if (panel.style.display === 'none') {
        panel.style.display = 'flex';
    } else {
        panel.style.display = 'none';
    }
}

function switchTab(tabName) {
    // Reset active classes
    document.querySelectorAll('.context-tab').forEach(t => t.classList.remove('active'));
    document.querySelectorAll('.tab-content').forEach(c => c.style.display = 'none');
    
    // Set active
    event.target.classList.add('active');
    document.getElementById(tabName + 'Tab').style.display = 'block';
}

function setView(mode) {
    // Reset Buttons
    document.querySelectorAll('.view-btn').forEach(b => b.classList.remove('active'));
    event.currentTarget.classList.add('active');

    const left = document.getElementById('leftPanel');
    const right = document.getElementById('rightPanel');
    const sidebar = document.getElementById('mainSidebar');

    if (mode === 'focus') {
        left.style.display = 'none';
        right.style.display = 'none';
        sidebar.style.display = 'none'; // Full focus
    } else if (mode === 'workbench') {
        left.style.display = 'flex';
        right.style.display = 'flex';
        sidebar.style.display = 'flex';
    }
}

function switchTheme() {
    const body = document.body;
    if (state.theme === 'dark') {
        body.setAttribute('data-theme', 'light');
        state.theme = 'light';
    } else {
        body.setAttribute('data-theme', 'professional');
        state.theme = 'dark';
    }
}

// --- RESIZE LOGIC ---
let isResizing = false;
let currentResizer = null;

document.querySelectorAll('.panel-resize-handle').forEach(handle => {
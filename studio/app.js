// GLOBAL STATE
const state = {
    isGenerating: false,
    controller: null,
    startTime: 0,
    tokenCount: 0,
    theme: localStorage.getItem('theme') || 'light',
    history: [], // {role: 'user'|'model', content: '...'}
    autoListen: false, // Hands-free mode
    recognition: null
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
    historyLimit: document.getElementById('historyLimit'),
    statusDot: document.querySelector('.status-dot'),
    statusText: document.getElementById('statusText'),
    consoleLog: document.getElementById('consoleLog'),
    langSelect: document.getElementById('langSelect'),
    fileInput: document.getElementById('fileInput'),
    micBtn: document.getElementById('micBtn'),
    autoModeIndicator: document.getElementById('autoModeIndicator'),
    lastLatency: document.getElementById('lastLatency'),
    tokenCountDisplay: document.getElementById('tokenCountDisplay')
};

// --- INIT ---
document.addEventListener('DOMContentLoaded', () => {
    applyTheme(state.theme);
    checkHealth();
    setInterval(checkHealth, 8000);
    setupEventListeners();
    setupSpeechRecognition();
    setViewInternal('focus');
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

    if(els.sendBtn) els.sendBtn.addEventListener('click', () => sendMessage());
    if(els.stopBtn) els.stopBtn.addEventListener('click', stopGeneration);
    
    if(els.tempInput) els.tempInput.addEventListener('input', (e) => els.tempVal.textContent = e.target.value);
    
    if(els.langSelect) {
        els.langSelect.addEventListener('change', (e) => {
            const lang = e.target.value;
            if(state.recognition) state.recognition.lang = lang;
            if(lang === 'en-US') {
                els.systemPrompt.value = "You are a helpful, skilled, and professional AI assistant.";
            } else {
                els.systemPrompt.value = "Sen yardımsever, yetenekli ve profesyonel bir yapay zeka asistanısın.";
            }
            logToConsole(`Language switched to ${lang}`);
        });
    }

    if(els.fileInput) {
        els.fileInput.addEventListener('change', (e) => {
            const file = e.target.files[0];
            if (!file) return;
            
            const reader = new FileReader();
            reader.onload = (event) => {
                const content = event.target.result;
                const current = els.ragInput.value;
                els.ragInput.value = (current ? current + "\n\n" : "") + 
                    `--- FILE: ${file.name} ---\n${content}`;
                
                const rightPanel = document.getElementById('rightPanel');
                if(rightPanel.style.display === 'none') togglePanel('rightPanel');
                
                logToConsole(`File loaded: ${file.name} (${content.length} chars)`);
            };
            reader.readAsText(file);
        });
    }
}

// --- SPEECH RECOGNITION ---
function setupSpeechRecognition() {
    if (!('webkitSpeechRecognition' in window)) {
        els.micBtn.style.display = 'none';
        return;
    }

    const recognition = new webkitSpeechRecognition();
    recognition.lang = 'tr-TR'; 
    recognition.continuous = false;
    recognition.interimResults = false;

    recognition.onstart = () => {
        if (state.isGenerating) {
            recognition.stop();
            return;
        }
        els.micBtn.style.color = '#ef4444'; 
        els.micBtn.classList.add('pulse');
        els.userInput.placeholder = "Dinliyorum...";
    };

    recognition.onend = () => {
        els.micBtn.style.color = '';
        els.micBtn.classList.remove('pulse');
        els.userInput.placeholder = "Bir şeyler yazın...";
        
        if (state.autoListen && !state.isGenerating) {
            if (els.userInput.value.trim().length === 0) {
                setTimeout(() => {
                    if(!state.isGenerating && state.autoListen) recognition.start();
                }, 500); 
            } else {
                sendMessage();
            }
        }
    };

    recognition.onresult = (event) => {
        const transcript = event.results[0][0].transcript;
        els.userInput.value = transcript;
    };

    recognition.onerror = (event) => {
        console.error("Speech Error:", event.error);
    };

    state.recognition = recognition;

    els.micBtn.addEventListener('click', () => {
        if (state.autoListen) {
            state.autoListen = false;
            els.autoModeIndicator.classList.add('hidden');
            recognition.stop();
        } else {
            recognition.start();
        }
    });

    els.micBtn.addEventListener('dblclick', () => {
        state.autoListen = true;
        els.autoModeIndicator.classList.remove('hidden');
        recognition.start();
    });
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
    state.history.push({ role: "user", content: text });

    const aiMsgContent = appendMessage('ai', '<span class="cursor"></span>');
    
    setGeneratingState(true);
    state.controller = new AbortController();
    state.startTime = Date.now();
    state.tokenCount = 0;
    
    const messagesPayload = buildMessagePayload(text);

    try {
        const response = await fetch('/v1/chat/completions', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                messages: messagesPayload,
                temperature: parseFloat(els.tempInput.value),
                stream: true
            }),
            signal: state.controller.signal
        });

        if (!response.ok) {
             const errText = await response.text();
             throw new Error(`HTTP ${response.status}: ${errText || 'Servis Hatası'}`);
        }

        const reader = response.body.getReader();
        const decoder = new TextDecoder();
        let fullResponse = "";
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

        aiMsgContent.innerHTML = marked.parse(fullResponse);
        hljs.highlightAll();
        
        state.history.push({ role: "assistant", content: fullResponse });
        updateMetrics(Date.now() - state.startTime, state.tokenCount);

        if (state.autoListen && state.recognition) {
            setTimeout(() => {
                try { state.recognition.start(); } catch(e){}
            }, 1500); 
        }

    } catch (err) {
        if (err.name !== 'AbortError') {
            const errMsg = err.message === 'Failed to fetch' 
                ? 'Bağlantı Hatası: Servis kapalı veya ağ sorunu var.' 
                : err.message;
                
            aiMsgContent.innerHTML += `<br><div style="background: #fee2e2; border: 1px solid #ef4444; color: #b91c1c; padding: 10px; border-radius: 8px; margin-top: 8px; font-size: 0.9em;"><strong>⚠️ HATA:</strong> ${errMsg}</div>`;
            
            logToConsole(`Error: ${errMsg}`);
        }
    } finally {
        setGeneratingState(false);
        state.controller = null;
    }
}

function buildMessagePayload(lastUserText) {
    const payload = [];
    
    if (els.systemPrompt.value.trim()) {
        payload.push({ role: "system", content: els.systemPrompt.value });
    }

    if (els.ragInput.value.trim()) {
        payload.push({ 
            role: "user", 
            content: `Aşağıdaki bağlamı ve kuralları kullanarak cevapla:\n\n${els.ragInput.value.trim()}` 
        });
        payload.push({ role: "assistant", content: "Anlaşıldı." });
    }

    const limit = parseInt(els.historyLimit.value) || 10;
    const historySlice = state.history.slice(0, -1).slice(-limit); 
    
    historySlice.forEach(msg => {
        payload.push({ role: msg.role === 'user' ? 'user' : 'assistant', content: msg.content });
    });

    payload.push({ role: "user", content: lastUserText });

    return payload;
}

// --- UI HELPERS ---

function appendMessage(role, html) {
    const div = document.createElement('div');
    div.className = `message ${role}`;
    const icon = role === 'user' ? 'user' : 'robot';
    div.innerHTML = `
        <div class="avatar"><i class="fas fa-${icon}"></i></div>
        <div class="msg-content">${html}</div>
    `;
    els.chatMessages.appendChild(div);
    scrollToBottom();
    return div.querySelector('.msg-content');
}

function clearChat() {
    state.history = [];
    els.chatMessages.innerHTML = `
        <div class="welcome-screen">
            <div class="welcome-icon"><i class="fas fa-eraser"></i></div>
            <h2>Sohbet Temizlendi</h2>
        </div>`;
    logToConsole("Chat history cleared.");
}

function logToConsole(msg) {
    const div = document.createElement('div');
    div.className = 'log-entry';
    div.textContent = `> ${msg}`;
    els.consoleLog.prepend(div);
}

function setGeneratingState(active) {
    state.isGenerating = active;
    if(els.sendBtn) els.sendBtn.classList.toggle('hidden', active);
    if(els.stopBtn) els.stopBtn.classList.toggle('hidden', !active);
}

function updateMetrics(duration, tokens) {
    if(els.lastLatency) els.lastLatency.textContent = `${duration}ms`;
    if(els.tokenCountDisplay) els.tokenCountDisplay.textContent = `${tokens} tokens`;
}

function scrollToBottom() {
    els.chatMessages.scrollTop = els.chatMessages.scrollHeight;
}

async function checkHealth() {
    try {
        const res = await fetch('/health');
        if (res.ok) {
            els.statusDot.className = 'status-dot connected';
            els.statusText.textContent = 'Hazır';
        } else {
            els.statusDot.className = 'status-dot';
            els.statusText.textContent = 'Servis Bekleniyor...';
        }
    } catch(e) {
        els.statusDot.className = 'status-dot';
        els.statusText.textContent = 'Koptu';
    }
}

// --- VIEW EXPORTS ---
window.togglePanel = (id) => {
    const el = document.getElementById(id);
    el.style.display = el.style.display === 'none' ? 'flex' : 'none';
};
window.switchTab = (tab) => {
    document.querySelectorAll('.tab-content').forEach(el => el.style.display = 'none');
    document.getElementById(tab + 'Tab').style.display = 'block';
    if(event) {
        document.querySelectorAll('.context-tab').forEach(el => el.classList.remove('active'));
        event.target.classList.add('active');
    }
};
window.clearChat = clearChat;
window.switchTheme = () => {
    state.theme = state.theme === 'light' ? 'dark' : 'light';
    applyTheme(state.theme);
    localStorage.setItem('theme', state.theme);
};
function applyTheme(theme) {
    if (theme === 'dark') document.body.setAttribute('data-theme', 'dark');
    else document.body.removeAttribute('data-theme');
}
function setViewInternal(mode) {
    const left = document.getElementById('leftPanel');
    const right = document.getElementById('rightPanel');
    document.querySelectorAll('.view-btn').forEach(b => b.classList.remove('active'));
    
    if(mode === 'focus') {
        left.style.display = 'none';
        right.style.display = 'none';
        document.querySelector('button[onclick*="focus"]').classList.add('active');
    } else {
        left.style.display = 'flex';
        right.style.display = 'flex';
        document.querySelector('button[onclick*="workbench"]').classList.add('active');
    }
}
window.setView = setViewInternal;
function stopGeneration() { if(state.controller) state.controller.abort(); }
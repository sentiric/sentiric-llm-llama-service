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
    // Input Auto-resize & Enter
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
    
    // Settings
    if(els.tempInput) els.tempInput.addEventListener('input', (e) => els.tempVal.textContent = e.target.value);
    
    // Language Change
    if(els.langSelect) {
        els.langSelect.addEventListener('change', (e) => {
            const lang = e.target.value;
            if(state.recognition) state.recognition.lang = lang;
            // Otomatik System Prompt güncellemesi
            if(lang === 'en-US') {
                els.systemPrompt.value = "You are a helpful, skilled, and professional AI assistant.";
            } else {
                els.systemPrompt.value = "Sen yardımsever, yetenekli ve profesyonel bir yapay zeka asistanısın.";
            }
            logToConsole(`Language switched to ${lang}`);
        });
    }

    // File Upload (RAG)
    if(els.fileInput) {
        els.fileInput.addEventListener('change', (e) => {
            const file = e.target.files[0];
            if (!file) return;
            
            const reader = new FileReader();
            reader.onload = (event) => {
                const content = event.target.result;
                // Mevcut RAG içeriğine ekle
                const current = els.ragInput.value;
                els.ragInput.value = (current ? current + "\n\n" : "") + 
                    `--- FILE: ${file.name} ---\n${content}`;
                
                // Sağ paneli aç
                const rightPanel = document.getElementById('rightPanel');
                if(rightPanel.style.display === 'none') togglePanel('rightPanel');
                
                logToConsole(`File loaded: ${file.name} (${content.length} chars)`);
            };
            reader.readAsText(file);
        });
    }
}

// --- SPEECH RECOGNITION (STT) ---
function setupSpeechRecognition() {
    if (!('webkitSpeechRecognition' in window)) {
        els.micBtn.style.display = 'none';
        return;
    }

    const recognition = new webkitSpeechRecognition();
    recognition.lang = 'tr-TR'; // Default, will change with select
    recognition.continuous = false;
    recognition.interimResults = false;

    recognition.onstart = () => {
        els.micBtn.style.color = '#ef4444'; // Red
        els.micBtn.classList.add('pulse');
        els.userInput.placeholder = "Dinliyorum...";
    };

    recognition.onend = () => {
        els.micBtn.style.color = '';
        els.micBtn.classList.remove('pulse');
        els.userInput.placeholder = "Bir şeyler yazın...";
        
        // Eğer metin geldiyse gönder
        if (els.userInput.value.trim().length > 0 && state.autoListen) {
            sendMessage();
        }
    };

    recognition.onresult = (event) => {
        const transcript = event.results[0][0].transcript;
        els.userInput.value = transcript;
    };

    state.recognition = recognition;

    // Tek Tık: Klasik Bas-Konuş
    els.micBtn.addEventListener('click', () => {
        if (state.autoListen) {
            // Sürekli modu kapat
            state.autoListen = false;
            els.autoModeIndicator.classList.add('hidden');
            recognition.stop();
        } else {
            recognition.start();
        }
    });

    // Çift Tık: Sürekli (Hands-Free) Mod
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
    document.querySelector('.welcome-screen').style.display = 'none';

    // 1. Kullanıcı mesajını ekle
    appendMessage('user', text);
    state.history.push({ role: "user", content: text });

    // 2. AI "Düşünüyor" animasyonu
    const aiMsgContent = appendMessage('ai', '<span class="cursor"></span>');
    
    setGeneratingState(true);
    state.controller = new AbortController();
    state.startTime = Date.now();
    state.tokenCount = 0;
    
    // 3. Mesaj Geçmişini Hazırla (History + RAG + System)
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

        if (!response.ok) throw new Error(`API ${response.status}`);

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

        // Bitiş
        aiMsgContent.innerHTML = marked.parse(fullResponse);
        hljs.highlightAll();
        
        // Geçmişe Ekle
        state.history.push({ role: "assistant", content: fullResponse });
        updateMetrics(Date.now() - state.startTime, state.tokenCount);

        // AUTO-LISTEN RESTART (Hands-Free)
        if (state.autoListen && state.recognition) {
            setTimeout(() => {
                try { state.recognition.start(); } catch(e){}
            }, 1000); // 1sn bekle sonra tekrar dinle
        }

    } catch (err) {
        if (err.name !== 'AbortError') {
            aiMsgContent.innerHTML += `<br><span style="color:red">HATA: ${err.message}</span>`;
        }
    } finally {
        setGeneratingState(false);
        state.controller = null;
    }
}

function buildMessagePayload(lastUserText) {
    const payload = [];
    
    // 1. System Prompt
    if (els.systemPrompt.value.trim()) {
        payload.push({ role: "system", content: els.systemPrompt.value });
    }

    // 2. RAG Context (System mesajı gibi davranabilir veya user mesajına eklenebilir)
    // Biz burada ayrı bir 'system' veya 'user' injection olarak ekleyeceğiz.
    if (els.ragInput.value.trim()) {
        payload.push({ 
            role: "user", 
            content: `Aşağıdaki bağlamı kullanarak soruları cevapla:\n\n${els.ragInput.value.trim()}` 
        });
        payload.push({ role: "assistant", content: "Anlaşıldı. Bağlamı dikkate alarak cevaplayacağım." });
    }

    // 3. History (Son N mesaj)
    const limit = parseInt(els.historyLimit.value) || 10;
    // Son mesaj zaten state.history'de var, ama onu tekrar eklememek için slice alıyoruz
    // state.history şunları içerir: User, AI, User, AI...
    // Son eklediğimiz User mesajı state.history'de var.
    
    // History'den son (Limit) kadarını al, ama şu anki mesajı (sonuncuyu) hariç tut çünkü aşağıda özel işleyeceğiz
    const historySlice = state.history.slice(0, -1).slice(-limit); 
    
    historySlice.forEach(msg => {
        payload.push({ role: msg.role === 'user' ? 'user' : 'assistant', content: msg.content });
    });

    // 4. Son Mesaj
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
    els.sendBtn.classList.toggle('hidden', active);
    els.stopBtn.classList.toggle('hidden', !active);
}

function updateMetrics(duration, tokens) {
    els.lastLatency.textContent = `${duration}ms`;
    els.tokenCountDisplay.textContent = `${tokens} tokens`;
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
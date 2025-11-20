// --- GLOBAL CONFIG ---
const CONFIG = {
    maxHistory: 15,
    defaultTemp: 0.7
};

// --- STATE ---
const state = {
    isGenerating: false,
    controller: null,
    startTime: 0,
    tokenCount: 0,
    theme: localStorage.getItem('theme') || 'light',
    history: [],
    autoListen: false,
    recognition: null
};

// --- DOM ELEMENTS ---
const $ = (id) => document.getElementById(id);
const els = {
    input: $('userInput'),
    chatBox: $('chatContainer'),
    sendBtn: $('sendBtn'),
    stopBtn: $('stopBtn'),
    systemPrompt: $('systemPrompt'),
    ragInput: $('ragInput'),
    tempInput: $('tempInput'),
    tempDisplay: $('tempDisplay'),
    historyLimit: $('historyLimit'),
    consoleLog: $('consoleLog'),
    fileInput: $('fileInput'),
    micBtn: $('micBtn'),
    statusText: $('statusText'),
    statusDot: $('statusDot'),
    stats: {
        latency: $('latencyStat'),
        tokens: $('tokenStat'),
        voice: $('voiceStatus')
    }
};

// --- INIT ---
document.addEventListener('DOMContentLoaded', () => {
    applyTheme(state.theme);
    checkHealth();
    setInterval(checkHealth, 10000);
    setupEventListeners();
    setupSpeech();
});

// --- MARKDOWN SETUP (BUG FIX) ---
// Renderer'ı özelleştirmek yerine, parse sonrası DOM manipülasyonu yapacağız.
// Bu [object Object] hatasını kesin olarak engeller.

// --- EVENT LISTENERS ---
function setupEventListeners() {
    // Input Auto-Resize
    els.input.addEventListener('input', function() {
        this.style.height = 'auto';
        this.style.height = (this.scrollHeight) + 'px';
    });

    // Enter Key
    els.input.addEventListener('keydown', (e) => {
        if (e.key === 'Enter' && !e.shiftKey) {
            e.preventDefault();
            sendMessage();
        }
    });

    els.sendBtn.onclick = sendMessage;
    els.stopBtn.onclick = stopGeneration;

    // Settings
    els.tempInput.oninput = (e) => els.tempDisplay.textContent = e.target.value;

    // File Upload
    els.fileInput.onchange = async (e) => {
        const file = e.target.files[0];
        if(!file) return;
        const text = await file.text();
        els.ragInput.value = (els.ragInput.value ? els.ragInput.value + "\n\n" : "") + `--- ${file.name} ---\n${text}`;
        log(`Dosya Yüklendi: ${file.name}`, 'info');
        toggleSidebar('contextPanel'); // Mobilde de çalışır
    };

    // Language Switcher
    $('langSelect').onchange = (e) => {
        if(state.recognition) state.recognition.lang = e.target.value;
        if(e.target.value === 'en-US') {
            els.systemPrompt.value = "You are Sentiric, a helpful and professional AI assistant.";
        } else {
            els.systemPrompt.value = "Senin adın Sentiric. Sen, Sentiric ekosisteminin yardımsever ve profesyonel yapay zeka asistanısın.";
        }
    };
}

// --- CORE LOGIC ---
async function sendMessage() {
    const text = els.input.value.trim();
    if (!text || state.isGenerating) return;

    // UI Reset
    els.input.value = '';
    els.input.style.height = 'auto';
    $('emptyState').style.display = 'none';

    // User Message
    appendMessage('user', text);
    state.history.push({role: 'user', content: text});

    // AI Placeholder
    const aiBubble = appendMessage('ai', '<span class="typing-cursor">█</span>');
    
    setBusy(true);
    state.controller = new AbortController();
    state.startTime = Date.now();
    state.tokenCount = 0;

    // Payload Builder
    const payload = buildPayload(text);

    try {
        const response = await fetch('/v1/chat/completions', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify(payload),
            signal: state.controller.signal
        });

        if(!response.ok) throw new Error(await response.text());

        const reader = response.body.getReader();
        const decoder = new TextDecoder();
        let fullText = "";

        while(true) {
            const {done, value} = await reader.read();
            if(done) break;
            
            const chunk = decoder.decode(value, {stream: true});
            const lines = chunk.split('\n');
            
            for(const line of lines) {
                if(line.startsWith('data: ') && line !== 'data: [DONE]') {
                    try {
                        const json = JSON.parse(line.substring(6));
                        const content = json.choices[0]?.delta?.content;
                        if(content) {
                            fullText += content;
                            // Anlık render (basit text)
                            aiBubble.innerHTML = escapeHtml(fullText) + '<span class="typing-cursor">█</span>';
                            state.tokenCount++;
                            smartScroll();
                        }
                    } catch(e){}
                }
            }
        }

        // FINAL RENDER (Markdown + Highlight)
        // [object Object] hatasını önleyen güvenli yöntem:
        aiBubble.innerHTML = marked.parse(fullText);
        
        // Kod bloklarını süsle
        aiBubble.querySelectorAll('pre code').forEach(block => {
            hljs.highlightElement(block);
            addCodeHeader(block);
        });

        state.history.push({role: 'assistant', content: fullText});
        updateStats();
        log('Yanıt tamamlandı.', 'success');

        // Auto Listen Restart
        if(state.autoListen) setTimeout(() => safeStartMic(), 1500);

    } catch(err) {
        if(err.name !== 'AbortError') {
            aiBubble.innerHTML += `<br><div style="color:red; margin-top:8px">Error: ${err.message}</div>`;
            log(err.message, 'error');
        }
    } finally {
        setBusy(false);
        state.controller = null;
    }
}

// --- HELPERS ---

function buildPayload(lastMsg) {
    const msgs = [];
    if(els.systemPrompt.value) msgs.push({role: 'system', content: els.systemPrompt.value});
    if(els.ragInput.value) msgs.push({role: 'user', content: `Context:\n${els.ragInput.value}`});
    
    const limit = parseInt(els.historyLimit.value) || 10;
    const history = state.history.slice(0, -1).slice(-limit); // Son mesajı hariç tut, onu aşağıda ekliyoruz
    
    history.forEach(m => msgs.push(m));
    msgs.push({role: 'user', content: lastMsg});

    return {
        messages: msgs,
        temperature: parseFloat(els.tempInput.value),
        stream: true
    };
}

function appendMessage(role, html) {
    const div = document.createElement('div');
    div.className = `message ${role}`;
    div.innerHTML = `
        <div class="avatar"><i class="fas fa-${role==='user'?'user':'robot'}"></i></div>
        <div class="bubble">${html}</div>
    `;
    els.chatBox.appendChild(div);
    smartScroll();
    return div.querySelector('.bubble');
}

// Safe Markdown Render Helpers
function addCodeHeader(block) {
    const pre = block.parentElement;
    const lang = block.className.split(' ').find(c => c.startsWith('language-')) || 'text';
    const langName = lang.replace('language-', '').toUpperCase();
    
    const header = document.createElement('div');
    header.className = 'code-header';
    header.innerHTML = `
        <span>${langName}</span>
        <button class="copy-btn" onclick="copyToClipboard(this, \`${encodeURIComponent(block.innerText)}\`)">
            <i class="fas fa-copy"></i> Kopyala
        </button>
    `;
    pre.insertBefore(header, block);
}

window.copyToClipboard = (btn, textEncoded) => {
    const text = decodeURIComponent(textEncoded);
    navigator.clipboard.writeText(text);
    const original = btn.innerHTML;
    btn.innerHTML = '<i class="fas fa-check"></i> Kopyalandı';
    setTimeout(() => btn.innerHTML = original, 2000);
};

function smartScroll() {
    const el = els.chatBox;
    // Eğer kullanıcı yukarı bakıyorsa scroll yapma
    if(el.scrollHeight - el.scrollTop - el.clientHeight < 150) {
        el.scrollTop = el.scrollHeight;
    }
}

function setBusy(isBusy) {
    state.isGenerating = isBusy;
    els.sendBtn.classList.toggle('hidden', isBusy);
    els.stopBtn.classList.toggle('hidden', !isBusy);
}

function escapeHtml(text) {
    return text.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
}

function updateStats() {
    const duration = Date.now() - state.startTime;
    els.stats.latency.innerText = `${duration}ms`;
    els.stats.tokens.innerText = `${state.tokenCount} tokens`;
}

function log(msg, type='info') {
    const div = document.createElement('div');
    div.className = `log-line ${type}`;
    div.innerText = `[${new Date().toLocaleTimeString()}] ${msg}`;
    els.consoleLog.prepend(div);
}

// --- VOICE & CONNECTION ---
function setupSpeech() {
    if(!('webkitSpeechRecognition' in window)) {
        els.micBtn.style.display = 'none';
        return;
    }
    const recognition = new webkitSpeechRecognition();
    recognition.continuous = false;
    recognition.lang = 'tr-TR';
    
    recognition.onstart = () => {
        els.micBtn.style.color = 'red';
        els.micBtn.classList.add('pulse');
        els.stats.voice.classList.remove('hidden');
    };
    
    recognition.onend = () => {
        els.micBtn.style.color = '';
        els.micBtn.classList.remove('pulse');
        els.stats.voice.classList.add('hidden');
        if(state.autoListen && !state.isGenerating && els.input.value) sendMessage();
        else if(state.autoListen && !els.input.value) setTimeout(safeStartMic, 500);
    };

    recognition.onresult = (e) => els.input.value = e.results[0][0].transcript;
    state.recognition = recognition;

    els.micBtn.onclick = () => {
        if(state.autoListen) { state.autoListen=false; recognition.stop(); }
        else recognition.start();
    };
    els.micBtn.ondblclick = () => { state.autoListen=true; recognition.start(); };
}

function safeStartMic() {
    if(state.isGenerating || !state.autoListen) return;
    try { state.recognition.start(); } catch(e){}
}

async function checkHealth() {
    try {
        const res = await fetch('/health');
        if(res.ok) {
            els.statusDot.style.background = '#10b981';
            els.statusText.innerText = 'Hazır';
        }
    } catch(e) {
        els.statusDot.style.background = '#ef4444';
        els.statusText.innerText = 'Koptu';
    }
}

// --- UI TOGGLES ---
window.toggleSidebar = (id) => {
    const el = $(id);
    // Mobile Logic
    if(window.innerWidth <= 768) {
        el.classList.toggle('open');
        // Overlay ekle/kaldır
        let overlay = document.querySelector('.overlay');
        if(!overlay) {
            overlay = document.createElement('div');
            overlay.className = 'overlay';
            overlay.onclick = () => {
                $('settingsPanel').classList.remove('open');
                $('contextPanel').classList.remove('open');
                overlay.remove();
            };
            document.body.appendChild(overlay);
        } else {
            overlay.remove();
        }
    } else {
        // Desktop
        el.style.display = el.style.display === 'none' ? 'flex' : 'none';
    }
};

window.switchTab = (tab) => {
    $('ragTab').style.display = tab==='rag'?'block':'none';
    $('logsTab').style.display = tab==='logs'?'block':'none';
    document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
    event.target.classList.add('active');
};

window.toggleTheme = () => {
    state.theme = state.theme === 'light' ? 'dark' : 'light';
    applyTheme(state.theme);
    localStorage.setItem('theme', state.theme);
};

function applyTheme(t) { document.body.setAttribute('data-theme', t); }
window.stopGeneration = () => { if(state.controller) state.controller.abort(); };
window.clearChat = () => { 
    state.history = []; 
    els.chatBox.innerHTML = ''; 
    $('emptyState').style.display = 'flex'; 
};
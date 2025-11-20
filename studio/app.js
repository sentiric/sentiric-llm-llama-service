// GLOBAL STATE
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
const els = {
    userInput: document.getElementById('userInput'),
    chatMessages: document.getElementById('chatMessages'),
    sendBtn: document.getElementById('sendBtn'),
    stopBtn: document.getElementById('stopBtn'),
    // ... Diğer elementler aynı ...
    systemPrompt: document.getElementById('systemPrompt'),
    ragInput: document.getElementById('ragInput'),
    tempInput: document.getElementById('tempInput'),
    tempVal: document.getElementById('tempVal'),
    statusDot: document.querySelector('.status-dot'),
    statusText: document.getElementById('statusText'),
    consoleLog: document.getElementById('consoleLog'),
    fileInput: document.getElementById('fileInput'),
    micBtn: document.getElementById('micBtn'),
    autoModeIndicator: document.getElementById('autoModeIndicator'),
    lastLatency: document.getElementById('lastLatency'),
    tokenCountDisplay: document.getElementById('tokenCountDisplay')
};

// --- CUSTOM MARKDOWN RENDERER (PRO FEATURES) ---
const renderer = new marked.Renderer();

// Code Block Override
renderer.code = (code, language) => {
    const validLang = !!(language && hljs.getLanguage(language));
    const langClass = validLang ? language : 'plaintext';
    
    // Rastgele ID üret (Kopyalama işlemi için)
    const codeId = 'code-' + Math.random().toString(36).substr(2, 9);
    
    return `
    <div class="code-wrapper">
        <div class="code-header">
            <span class="lang-label">${langClass}</span>
            <button class="copy-code-btn" onclick="copyCode('${codeId}')">
                <i class="fas fa-copy"></i> Kopyala
            </button>
        </div>
        <pre><code id="${codeId}" class="hljs language-${langClass}">${validLang ? hljs.highlight(code, { language }).value : code}</code></pre>
    </div>`;
};

marked.setOptions({ renderer });

// --- INIT ---
document.addEventListener('DOMContentLoaded', () => {
    applyTheme(state.theme);
    checkHealth();
    setupEventListeners();
    setupSpeechRecognition();
    setViewInternal('focus');
});

// --- EVENT HANDLERS ---
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
    
    if(els.fileInput) {
        els.fileInput.addEventListener('change', (e) => {
            // Dosya yükleme mantığı (Önceki kod ile aynı)
            const file = e.target.files[0];
            if(!file) return;
            const reader = new FileReader();
            reader.onload = (ev) => {
                els.ragInput.value += `\n\n--- FILE: ${file.name} ---\n${ev.target.result}`;
                togglePanel('rightPanel'); // Panel otomatik açılsın
            };
            reader.readAsText(file);
        });
    }
    // Slider vb.
    if(els.tempInput) els.tempInput.addEventListener('input', (e) => els.tempVal.textContent = e.target.value);
}

// --- CORE LOGIC ---

async function sendMessage() {
    const text = els.userInput.value.trim();
    if (!text || state.isGenerating) return;

    els.userInput.value = '';
    els.userInput.style.height = 'auto';
    document.querySelector('.welcome-screen').style.display = 'none';

    // User Mesajı
    appendMessage('user', text);
    state.history.push({ role: "user", content: text });

    // AI Mesaj Hazırlığı
    const aiMsgDiv = document.createElement('div');
    aiMsgDiv.className = 'message ai';
    aiMsgDiv.innerHTML = `
        <div class="avatar"><i class="fas fa-robot"></i></div>
        <div class="msg-content"><span class="cursor"></span></div>
    `;
    els.chatMessages.appendChild(aiMsgDiv);
    const contentSpan = aiMsgDiv.querySelector('.msg-content');

    setGeneratingState(true);
    state.controller = new AbortController();
    state.startTime = Date.now();
    state.tokenCount = 0;
    
    // Payload Hazırla
    const payload = [];
    if(els.systemPrompt.value) payload.push({role: "system", content: els.systemPrompt.value});
    if(els.ragInput.value) payload.push({role: "user", content: "Context:\n" + els.ragInput.value});
    // Geçmişi ekle
    state.history.slice(-10).forEach(h => payload.push(h));

    try {
        const response = await fetch('/v1/chat/completions', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                messages: payload,
                temperature: parseFloat(els.tempInput.value),
                stream: true
            }),
            signal: state.controller.signal
        });

        const reader = response.body.getReader();
        const decoder = new TextDecoder();
        let fullResponse = "";

        while (true) {
            const { done, value } = await reader.read();
            if (done) break;
            
            const chunk = decoder.decode(value, { stream: true });
            const lines = chunk.split('\n');
            
            for (const line of lines) {
                if (line.startsWith('data: ') && line !== 'data: [DONE]') {
                    try {
                        const json = JSON.parse(line.substring(6));
                        const delta = json.choices[0]?.delta?.content;
                        if (delta) {
                            fullResponse += delta;
                            // Render (Markdown)
                            // Performans için her 10 tokenda bir tam render yapılabilir, şimdilik direkt yapıyoruz
                            contentSpan.innerHTML = marked.parse(fullResponse) + '<span class="cursor"></span>';
                            state.tokenCount++;
                            smartScroll();
                        }
                    } catch (e) {}
                }
            }
        }

        // Bitiş
        contentSpan.innerHTML = marked.parse(fullResponse);
        state.history.push({ role: "assistant", content: fullResponse });
        
        // Aksiyon Butonlarını Ekle
        addMessageActions(contentSpan, fullResponse);
        
        updateMetrics(Date.now() - state.startTime, state.tokenCount);
        
        // Hands-free loop
        if(state.autoListen && state.recognition) setTimeout(() => state.recognition.start(), 1000);

    } catch (err) {
        if (err.name !== 'AbortError') contentSpan.innerHTML += `<br><span style="color:red">Error: ${err.message}</span>`;
    } finally {
        setGeneratingState(false);
        state.controller = null;
    }
}

// --- FEATURES ---

// Kodu Kopyala
window.copyCode = function(id) {
    const codeBlock = document.getElementById(id);
    const btn = codeBlock.parentElement.previousElementSibling.querySelector('.copy-code-btn');
    
    navigator.clipboard.writeText(codeBlock.innerText).then(() => {
        const originalHtml = btn.innerHTML;
        btn.innerHTML = '<i class="fas fa-check"></i> Kopyalandı';
        btn.style.color = '#10b981';
        setTimeout(() => {
            btn.innerHTML = originalHtml;
            btn.style.color = '';
        }, 2000);
    });
};

// Mesaj Aksiyonları
function addMessageActions(container, text) {
    const actionsDiv = document.createElement('div');
    actionsDiv.className = 'message-actions';
    actionsDiv.innerHTML = `
        <button class="action-btn" onclick="navigator.clipboard.writeText(decodeURIComponent('${encodeURIComponent(text)}'))">
            <i class="fas fa-copy"></i> Kopyala
        </button>
        <button class="action-btn" onclick="sendMessage()">
            <i class="fas fa-redo"></i> Yeniden
        </button>
    `;
    container.appendChild(actionsDiv);
}

// Akıllı Scroll
function smartScroll() {
    const el = els.chatMessages;
    const isNearBottom = el.scrollHeight - el.scrollTop - el.clientHeight < 100;
    if (isNearBottom) el.scrollTop = el.scrollHeight;
}

// Diğer Yardımcılar
function appendMessage(role, text) {
    const div = document.createElement('div');
    div.className = `message ${role}`;
    div.innerHTML = `
        <div class="avatar"><i class="fas fa-${role === 'user' ? 'user' : 'robot'}"></i></div>
        <div class="msg-content">${text}</div>
    `;
    els.chatMessages.appendChild(div);
    smartScroll();
}

function setGeneratingState(active) {
    state.isGenerating = active;
    if(active) {
        els.sendBtn.classList.add('hidden');
        els.stopBtn.classList.remove('hidden');
    } else {
        els.sendBtn.classList.remove('hidden');
        els.stopBtn.classList.add('hidden');
        els.userInput.focus();
    }
}

function updateMetrics(dur, tok) {
    if(els.lastLatency) els.lastLatency.textContent = `${dur}ms`;
    if(els.tokenCountDisplay) els.tokenCountDisplay.textContent = `${tok} tokens`;
}

async function checkHealth() {
    try {
        const res = await fetch('/health');
        if(res.ok) {
            els.statusDot.className = 'status-dot connected';
            els.statusText.textContent = 'Hazır';
        }
    } catch(e) {
        els.statusDot.className = 'status-dot';
        els.statusText.textContent = 'Koptu';
    }
}

// Speech Recognition (Önceki kodun aynısı, kısaltıldı)
function setupSpeechRecognition() {
    if (!('webkitSpeechRecognition' in window)) { els.micBtn.style.display = 'none'; return; }
    const recognition = new webkitSpeechRecognition();
    recognition.lang = 'tr-TR';
    recognition.continuous = false;
    recognition.onstart = () => { els.micBtn.style.color = 'red'; els.userInput.placeholder = "Dinliyorum..."; };
    recognition.onend = () => { els.micBtn.style.color = ''; els.userInput.placeholder = "Mesaj yazın..."; if(state.autoListen && !state.isGenerating && els.userInput.value) sendMessage(); };
    recognition.onresult = (e) => els.userInput.value = e.results[0][0].transcript;
    state.recognition = recognition;
    els.micBtn.addEventListener('click', () => state.autoListen ? (state.autoListen=false, recognition.stop(), els.autoModeIndicator.classList.add('hidden')) : recognition.start());
    els.micBtn.addEventListener('dblclick', () => { state.autoListen=true; els.autoModeIndicator.classList.remove('hidden'); recognition.start(); });
}

// View & Theme Logic
window.togglePanel = (id) => { const el=document.getElementById(id); el.style.display = el.style.display==='none'?'flex':'none'; };
window.switchTab = (tab) => { document.querySelectorAll('.tab-content').forEach(e=>e.style.display='none'); document.getElementById(tab+'Tab').style.display='block'; if(event) { document.querySelectorAll('.context-tab').forEach(e=>e.classList.remove('active')); event.target.classList.add('active'); } };
window.clearChat = () => { state.history=[]; els.chatMessages.innerHTML = '<div class="welcome-screen"><h2>Temizlendi</h2></div>'; };
window.switchTheme = () => { state.theme = state.theme==='light'?'dark':'light'; applyTheme(state.theme); localStorage.setItem('theme', state.theme); };
function applyTheme(t) { document.body.setAttribute('data-theme', t); }
function setViewInternal(mode) { const l=document.getElementById('leftPanel'), r=document.getElementById('rightPanel'); document.querySelectorAll('.view-btn').forEach(b=>b.classList.remove('active')); if(mode==='focus'){ l.style.display='none'; r.style.display='none'; } else { l.style.display='flex'; r.style.display='flex'; } }
window.setView = setViewInternal;
function stopGeneration() { if(state.controller) state.controller.abort(); }
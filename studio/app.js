// --- STATE & UTILS ---
const $ = (id) => document.getElementById(id);
const state = { 
    generating: false, controller: null, history: [], 
    autoListen: false, recognition: null, startTime: 0, tokenCount: 0 
};

// --- INIT ---
document.addEventListener('DOMContentLoaded', () => {
    const theme = localStorage.getItem('theme') || 'light';
    document.body.setAttribute('data-theme', theme);
    
    setupEvents();
    setupSpeech();
    checkHealth();
    setInterval(checkHealth, 10000);
});

// --- EVENTS ---
function setupEvents() {
    const input = $('userInput');
    
    // Auto-Resize Textarea
    input.addEventListener('input', function() {
        this.style.height = 'auto';
        this.style.height = Math.min(this.scrollHeight, 150) + 'px';
    });

    // Enter to Send
    input.addEventListener('keydown', (e) => {
        if (e.key === 'Enter' && !e.shiftKey) {
            e.preventDefault();
            sendMessage();
        }
    });

    $('sendBtn').onclick = sendMessage;
    $('stopBtn').onclick = () => state.controller?.abort();
    
    // Settings Bindings
    $('tempInput').oninput = (e) => $('tempVal').innerText = e.target.value;
    $('historyLimit').oninput = (e) => $('historyVal').innerText = e.target.value;
    
    // File Upload
    $('fileInput').onchange = async (e) => {
        const file = e.target.files[0];
        if(!file) return;
        const text = await file.text();
        const rag = $('ragInput');
        rag.value = (rag.value ? rag.value + "\n\n" : "") + `--- ${file.name} ---\n${text}`;
        log(`Dosya eklendi: ${file.name}`);
        // Auto open right panel
        togglePanel('rightPanel', true);
    };

    // Language
    $('langSelect').onchange = (e) => {
        const isEn = e.target.value === 'en-US';
        if(state.recognition) state.recognition.lang = e.target.value;
        $('systemPrompt').value = isEn 
            ? ""
            : "";
    };
}

// --- CORE LLM LOGIC ---
async function sendMessage() {
    const text = $('userInput').value.trim();
    if (!text || state.generating) return;

    // UI Reset
    $('userInput').value = '';
    $('userInput').style.height = 'auto';
    $('emptyState').style.display = 'none';
    
    // User Message
    addMessage('user', text);
    state.history.push({role: 'user', content: text});

    // AI Placeholder
    const aiBubble = addMessage('ai', '<span class="cursor"></span>');
    
    setBusy(true);
    state.controller = new AbortController();
    state.startTime = Date.now();
    state.tokenCount = 0;

    // Payload
    const payload = buildPayload(text);
    log(`İstek gönderiliyor (${payload.messages.length} msj)...`);

    try {
        const response = await fetch('/v1/chat/completions', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify(payload),
            signal: state.controller.signal
        });

        if (!response.ok) throw new Error(await response.text());

        const reader = response.body.getReader();
        const decoder = new TextDecoder();
        let fullText = "";

        while(true) {
            const {done, value} = await reader.read();
            if(done) break;
            
            const chunk = decoder.decode(value);
            const lines = chunk.split('\n');
            
            for(const line of lines) {
                if(line.startsWith('data: ') && line !== 'data: [DONE]') {
                    try {
                        const json = JSON.parse(line.substring(6));
                        const content = json.choices[0]?.delta?.content;
                        if(content) {
                            fullText += content;
                            // Basic render for speed
                            aiBubble.innerHTML = escapeHtml(fullText) + '<span class="cursor"></span>';
                            state.tokenCount++;
                            smartScroll();
                        }
                    } catch(e){}
                }
            }
        }

        // Final Polish (Markdown + Highlight)
        aiBubble.innerHTML = marked.parse(fullText);
        hljs.highlightAll();
        enhanceCodeBlocks(aiBubble);
        
        state.history.push({role: 'assistant', content: fullText});
        updateStats();
        log(`Yanıt tamamlandı (${state.tokenCount} token).`);

        if(state.autoListen) setTimeout(tryStartMic, 1500);

    } catch(err) {
        if(err.name !== 'AbortError') {
            aiBubble.innerHTML += `<br><div style="color:var(--danger); margin-top:8px"><strong>Hata:</strong> ${err.message}</div>`;
            log(`Hata: ${err.message}`);
        }
    } finally {
        setBusy(false);
    }
}

// --- HELPERS ---

function buildPayload(lastMsg) {
    const msgs = [];
    const sys = $('systemPrompt').value;
    const rag = $('ragInput').value;
    
    if(sys) msgs.push({role: 'system', content: sys});
    if(rag) msgs.push({role: 'user', content: `BAĞLAM BİLGİSİ:\n${rag}\n\n(Bağlamı kullanarak cevapla)`});
    if(rag) msgs.push({role: 'assistant', content: 'Anlaşıldı.'});
    
    const limit = parseInt($('historyLimit').value) || 10;
    // Son mesajı hariç tut, slice yap
    state.history.slice(-limit).forEach(m => msgs.push(m));
    
    // Son mesaj zaten history'e eklendi ama payload'a tekrar eklemiyoruz, 
    // yukarıdaki slice son mesajı da içerir eğer push yapıldıysa.
    // Düzeltme: sendMessage içinde push yapıyoruz. O yüzden slice(-limit) yeterli.
    
    return {
        messages: msgs,
        temperature: parseFloat($('tempInput').value),
        stream: true
    };
}

function addMessage(role, html) {
    const div = document.createElement('div');
    div.className = `message ${role}`;
    div.innerHTML = `
        <div class="avatar">
            <i class="fas fa-${role==='user'?'user':'robot'}"></i>
        </div>
        <div class="bubble">${html}</div>
    `;
    $('chatContainer').appendChild(div);
    smartScroll();
    return div.querySelector('.bubble');
}

function enhanceCodeBlocks(el) {
    el.querySelectorAll('pre').forEach(pre => {
        const code = pre.querySelector('code');
        if(!code) return;
        
        // Header ekle
        const header = document.createElement('div');
        header.className = 'code-header';
        header.innerHTML = `
            <span>Code</span>
            <button class="action-icon" style="font-size:0.8rem; padding:4px" onclick="copyText(this, '${encodeURIComponent(pre.innerText)}')">
                <i class="fas fa-copy"></i> Kopyala
            </button>
        `;
        pre.insertBefore(header, code);
    });
}

window.copyText = (btn, text) => {
    navigator.clipboard.writeText(decodeURIComponent(text));
    const original = btn.innerHTML;
    btn.innerHTML = '<i class="fas fa-check"></i>';
    setTimeout(() => btn.innerHTML = original, 2000);
};

function smartScroll() {
    const el = $('chatContainer');
    const isNearBottom = el.scrollHeight - el.scrollTop - el.clientHeight < 150;
    if(isNearBottom) el.scrollTop = el.scrollHeight;
}

function setBusy(busy) {
    state.generating = busy;
    $('sendBtn').classList.toggle('hidden', busy);
    $('stopBtn').classList.toggle('hidden', !busy);
}

function updateStats() {
    const dur = Date.now() - state.startTime;
    $('latencyVal').innerText = `${dur}ms`;
    $('tokenVal').innerText = state.tokenCount;
    const tps = (state.tokenCount / (dur/1000)).toFixed(1);
    $('tpsVal').innerText = tps;
}

function log(msg) {
    const d = document.createElement('div');
    d.className = 'log-line';
    d.innerText = `[${new Date().toLocaleTimeString()}] ${msg}`;
    $('consoleLog').prepend(d);
}

function escapeHtml(text) {
    return text.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
}

// --- TOGGLES ---
window.togglePanel = (id, forceOpen = false) => {
    const el = $(id);
    if(window.innerWidth <= 768) {
        // Mobile Logic
        el.classList.toggle('active', forceOpen || !el.classList.contains('active'));
        // Overlay
        let ov = document.querySelector('.overlay');
        if(el.classList.contains('active')) {
            if(!ov) {
                ov = document.createElement('div'); ov.className='overlay';
                ov.onclick = () => { 
                    $('leftPanel').classList.remove('active'); 
                    $('rightPanel').classList.remove('active'); 
                    ov.remove(); 
                };
                document.body.appendChild(ov);
            }
        } else {
            if(ov && !$('leftPanel').classList.contains('active') && !$('rightPanel').classList.contains('active')) ov.remove();
        }
    } else {
        // Desktop Logic (Margin manipulation)
        if(id === 'leftPanel') el.classList.toggle('closed');
        if(id === 'rightPanel') el.classList.toggle('closed');
    }
};

window.switchTab = (tab) => {
    $('ragTab').style.display = tab==='rag'?'block':'none';
    $('logsTab').style.display = tab==='logs'?'block':'none';
    const tabs = document.querySelectorAll('.tab');
    tabs.forEach(t => t.classList.remove('active'));
    event.target.classList.add('active');
};

window.toggleTheme = () => {
    const current = document.body.getAttribute('data-theme');
    const next = current === 'light' ? 'dark' : 'light';
    document.body.setAttribute('data-theme', next);
    localStorage.setItem('theme', next);
};

window.clearChat = () => {
    state.history = [];
    $('chatContainer').innerHTML = `
        <div class="empty-state" id="emptyState">
            <div class="logo-shine"><i class="fas fa-check-circle"></i></div>
            <h2>Temizlendi</h2>
        </div>`;
    log("Sohbet temizlendi.");
};

// --- SPEECH ---
function setupSpeech() {
    if(!('webkitSpeechRecognition' in window)) { $('micBtn').style.display='none'; return; }
    const rec = new webkitSpeechRecognition();
    rec.lang = 'tr-TR';
    rec.onstart = () => { 
        $('micBtn').classList.add('pulse'); 
        $('voiceStatus').classList.remove('hidden');
    };
    rec.onend = () => { 
        $('micBtn').classList.remove('pulse');
        $('voiceStatus').classList.add('hidden');
        if(state.autoListen && !state.generating && $('userInput').value) sendMessage();
        else if(state.autoListen) setTimeout(tryStartMic, 500);
    };
    rec.onresult = (e) => $('userInput').value = e.results[0][0].transcript;
    state.recognition = rec;

    $('micBtn').onclick = () => {
        if(state.autoListen) { state.autoListen=false; rec.stop(); }
        else rec.start();
    };
    $('micBtn').ondblclick = () => { state.autoListen=true; rec.start(); };
}
function tryStartMic() { try{state.recognition.start();}catch(e){} }

async function checkHealth() {
    try {
        const res = await fetch('/health');
        if(res.ok) {
            $('statusText').innerText = 'Hazır';
            $('connStatus').style.borderColor = 'var(--success)';
        }
    } catch(e) {
        $('statusText').innerText = 'Koptu';
        $('connStatus').style.borderColor = 'var(--danger)';
    }
}
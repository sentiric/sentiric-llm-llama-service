const $ = (id) => document.getElementById(id);
const state = { generating: false, controller: null, history: [], autoListen: false };

// --- INIT ---
document.addEventListener('DOMContentLoaded', () => {
    const theme = localStorage.getItem('theme') || 'light';
    document.body.setAttribute('data-theme', theme);
    
    setupEvents();
    setupSpeech();
    
    // Input Focus
    setTimeout(() => $('userInput').focus(), 100);
});

// --- EVENTS ---
function setupEvents() {
    const input = $('userInput');
    
    input.oninput = function() {
        this.style.height = 'auto';
        this.style.height = Math.min(this.scrollHeight, 150) + 'px';
    };

    input.onkeydown = (e) => {
        if (e.key === 'Enter' && !e.shiftKey) {
            e.preventDefault();
            sendMessage();
        }
    };

    $('sendBtn').onclick = sendMessage;
    $('stopBtn').onclick = () => state.controller?.abort();
    $('fileInput').onchange = handleFile;
    $('tempInput').oninput = (e) => $('tempVal').innerText = e.target.value;
}

// --- CORE ---
async function sendMessage() {
    const text = $('userInput').value.trim();
    if (!text || state.generating) return;

    // UI Reset
    $('userInput').value = '';
    $('userInput').style.height = 'auto';
    $('emptyState').style.display = 'none';
    
    // 1. User Message
    addMessage('user', text);
    state.history.push({role: 'user', content: text});

    // 2. AI "Thinking" Placeholder
    const aiBubble = addMessage('ai', '<span class="thinking">Sentiric düşünüyor...</span>');
    
    toggleBusy(true);
    state.controller = new AbortController();
    const startTime = Date.now();
    let tokenCount = 0;

    // Payload
    const payload = {
        messages: [
            {role: 'system', content: $('systemPrompt').value},
            {role: 'user', content: $('ragInput').value ? `Context:\n${$('ragInput').value}\n\nQuestion:\n${text}` : text}
        ],
        temperature: parseFloat($('tempInput').value),
        stream: true
    };

    try {
        const res = await fetch('/v1/chat/completions', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify(payload),
            signal: state.controller.signal
        });

        const reader = res.body.getReader();
        const decoder = new TextDecoder();
        let fullText = "";
        let firstToken = true;

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
                            // İlk token geldiğinde "Thinking" yazısını sil
                            if(firstToken) {
                                aiBubble.innerHTML = "";
                                firstToken = false;
                            }
                            fullText += content;
                            // Basit ekleme (Performans için markdown'u sonda yapıyoruz)
                            aiBubble.innerText = fullText; 
                            tokenCount++;
                            scrollToBottom();
                        }
                    } catch(e){}
                }
            }
        }

        // Final Markdown Render
        aiBubble.innerHTML = marked.parse(fullText);
        hljs.highlightAll();
        addCopyButtons(aiBubble);
        
        $('latencyStat').innerText = `${Date.now() - startTime}ms`;
        $('tokenStat').innerText = `${tokenCount} t`;

        // Auto mic
        if(state.autoListen) setTimeout(() => tryStartMic(), 1000);

    } catch(err) {
        if(err.name !== 'AbortError') aiBubble.innerHTML += `<br><span style="color:var(--danger)">Hata: ${err.message}</span>`;
    } finally {
        toggleBusy(false);
    }
}

// --- UI HELPERS ---
function addMessage(role, html) {
    const div = document.createElement('div');
    div.className = `message ${role}`;
    div.innerHTML = `
        <div class="message-avatar ${role}-avatar">
            <i class="fas fa-${role==='user'?'user':'robot'}"></i>
        </div>
        <div class="bubble">${html}</div>
    `;
    $('chatContainer').appendChild(div);
    scrollToBottom();
    return div.querySelector('.bubble');
}

function scrollToBottom() {
    const el = $('chatContainer');
    // Kullanıcı yukarı bakıyorsa zorla indirme
    if (el.scrollHeight - el.scrollTop - el.clientHeight < 150) {
        el.scrollTop = el.scrollHeight;
    }
}

function toggleBusy(busy) {
    state.generating = busy;
    $('sendBtn').classList.toggle('hidden', busy);
    $('stopBtn').classList.toggle('hidden', !busy);
}

function addCopyButtons(el) {
    el.querySelectorAll('pre').forEach(pre => {
        const btn = document.createElement('button');
        btn.className = 'copy-btn';
        btn.innerHTML = '<i class="fas fa-copy"></i> Copy';
        btn.style.position = 'absolute';
        btn.style.top = '5px';
        btn.style.right = '5px';
        btn.style.color = '#aaa';
        btn.style.background = 'rgba(255,255,255,0.1)';
        btn.style.border = 'none';
        btn.style.borderRadius = '4px';
        btn.style.padding = '4px 8px';
        btn.style.cursor = 'pointer';
        
        btn.onclick = () => {
            navigator.clipboard.writeText(pre.innerText);
            btn.innerHTML = '<i class="fas fa-check"></i>';
            setTimeout(() => btn.innerHTML = '<i class="fas fa-copy"></i> Copy', 2000);
        };
        pre.style.position = 'relative';
        pre.appendChild(btn);
    });
}

// --- SIDEBARS & TOGGLES ---
window.toggleSidebar = (id) => {
    const el = $(id);
    el.classList.toggle('closed');
    // Overlay for mobile
    if(window.innerWidth < 768 && !el.classList.contains('closed')) {
        const overlay = document.createElement('div');
        overlay.className = 'overlay';
        overlay.onclick = () => {
            el.classList.add('closed');
            overlay.remove();
        };
        document.body.appendChild(overlay);
    }
};

window.clearChat = () => {
    $('chatContainer').innerHTML = '<div class="empty-state" style="text-align:center; margin-top:30vh; opacity:0.6"><i class="fas fa-bolt" style="font-size:3rem; margin-bottom:1rem; color:var(--border)"></i><h2>Temizlendi</h2></div>';
};

window.toggleTheme = () => {
    const current = document.body.getAttribute('data-theme');
    const next = current === 'light' ? 'dark' : 'light';
    document.body.setAttribute('data-theme', next);
    localStorage.setItem('theme', next);
};

async function handleFile(e) {
    const file = e.target.files[0];
    if(!file) return;
    const text = await file.text();
    $('ragInput').value += `\n\n--- ${file.name} ---\n${text}`;
    log(`Dosya Yüklendi: ${file.name}`);
    $('rightPanel').classList.remove('closed');
}

function log(msg) {
    const d = document.createElement('div');
    d.innerText = `> ${msg}`;
    $('consoleLog').prepend(d);
}

// --- SPEECH ---
function setupSpeech() {
    if(!('webkitSpeechRecognition' in window)) { $('micBtn').style.display='none'; return; }
    const rec = new webkitSpeechRecognition();
    rec.lang = 'tr-TR';
    rec.onstart = () => $('micBtn').classList.add('mic-active');
    rec.onend = () => {
        $('micBtn').classList.remove('mic-active');
        if(state.autoListen && !state.generating && $('userInput').value) sendMessage();
    };
    rec.onresult = (e) => $('userInput').value = e.results[0][0].transcript;
    state.recognition = rec;
    
    $('micBtn').onclick = () => {
        if(state.autoListen) { state.autoListen=false; rec.stop(); }
        else rec.start();
    };
    $('micBtn').ondblclick = () => { state.autoListen=true; rec.start(); };
}
function tryStartMic() { try{ state.recognition.start(); }catch(e){} }
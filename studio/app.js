// --- STATE ---
const $ = (id) => document.getElementById(id);
const state = { 
    generating: false, 
    controller: null, 
    history: [], 
    autoListen: false, 
    recognition: null, 
    startTime: 0, 
    tokenCount: 0,
    autoScroll: true,
    interrupted: false // Yeni Flag: Söz kesildi mi?
};

// --- INIT ---
document.addEventListener('DOMContentLoaded', () => {
    const theme = localStorage.getItem('theme') || 'light';
    document.body.setAttribute('data-theme', theme);
    
    setupEvents();
    setupSpeech();
    setupMarkdown();
    checkHealth();
    setInterval(checkHealth, 10000);

    addMessage('ai', 'Merhaba! "Barge-in" modu aktif. Ben konuşurken sözümü kesebilirsiniz, sizi dinliyorum.');
});

function setupMarkdown() {
    marked.setOptions({
        highlight: function(code, lang) {
            const language = hljs.getLanguage(lang) ? lang : 'plaintext';
            return hljs.highlight(code, { language }).value;
        },
        langPrefix: 'hljs language-'
    });
}

// --- EVENTS ---
function setupEvents() {
    const input = $('userInput');
    
    input.addEventListener('input', function() {
        this.style.height = 'auto';
        this.style.height = Math.min(this.scrollHeight, 200) + 'px';
    });

    input.addEventListener('keydown', (e) => {
        if (e.key === 'Enter' && !e.shiftKey) {
            e.preventDefault();
            if(window.innerWidth < 768) input.blur();
            // Manuel girişte söz kesme mantığı
            if (state.generating) interruptGeneration();
            sendMessage();
        }
    });

    $('sendBtn').onclick = () => {
        state.autoListen = false;
        stopMicUI();
        if (state.generating) interruptGeneration();
        sendMessage();
    };
    
    $('stopBtn').onclick = () => {
        state.autoListen = false;
        interruptGeneration();
        stopMicUI();
    };
    
    $('tempInput').oninput = (e) => $('tempVal').innerText = e.target.value;
    $('tokenLimit').oninput = (e) => $('tokenLimitVal').innerText = e.target.value;
    $('historyLimit').oninput = (e) => $('historyVal').innerText = e.target.value;
    $('ragInput').oninput = (e) => $('ragCharCount').innerText = e.target.value.length;

    $('chatContainer').addEventListener('scroll', function() {
        const isAtBottom = this.scrollHeight - this.scrollTop - this.clientHeight < 50;
        state.autoScroll = isAtBottom;
        $('scrollBtn').classList.toggle('hidden', isAtBottom);
    });

    $('fileInput').onchange = async (e) => {
        const file = e.target.files[0];
        if(!file) return;
        try {
            const text = await file.text();
            const rag = $('ragInput');
            rag.value = (rag.value ? rag.value + "\n\n" : "") + `--- ${file.name} ---\n${text}`;
            $('ragCharCount').innerText = rag.value.length;
            togglePanel('rightPanel', true);
            switchTab('rag');
        } catch(err) {
            alert("Dosya okunamadı: " + err.message);
        }
    };
}

// --- INTERRUPT LOGIC (YENİ) ---
function interruptGeneration() {
    if (state.controller) {
        state.interrupted = true;
        state.controller.abort(); // Backend'e "Dur" sinyali gönderir
        state.controller = null;
    }
}

// --- CORE LOGIC ---
async function sendMessage() {
    const text = $('userInput').value.trim();
    if (!text) return;

    // Eğer önceki işlem hala sürüyorsa ve buraya geldiysek, interrupt edilmiştir.
    
    $('userInput').value = '';
    $('userInput').style.height = 'auto';
    $('emptyState').style.display = 'none';
    state.autoScroll = true;
    state.interrupted = false;
    
    addMessage('user', escapeHtml(text));
    state.history.push({role: 'user', content: text});

    const aiBubble = addMessage('ai', '<span class="cursor"></span>');
    const bubbleContent = aiBubble.querySelector('.markdown-content');
    
    setBusy(true);
    state.controller = new AbortController();
    state.startTime = Date.now();
    state.tokenCount = 0;

    const payload = buildPayload(text);

    // --- BARGE-IN MANTIĞI: HEMEN DİNLEMEYE BAŞLA ---
    // İstek gönderilirken mikrofonu açık tutuyoruz ki kullanıcı araya girebilsin.
    if (state.autoListen) {
        tryStartMic();
    }

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
        let buffer = "";

        while(true) {
            const {done, value} = await reader.read();
            if(done) break;
            
            buffer += decoder.decode(value, {stream: true});
            const lines = buffer.split('\n');
            buffer = lines.pop();

            for(const line of lines) {
                if(line.startsWith('data: ') && line !== 'data: [DONE]') {
                    try {
                        const json = JSON.parse(line.substring(6));
                        const content = json.choices[0]?.delta?.content;
                        if(content) {
                            fullText += content;
                            bubbleContent.innerHTML = marked.parse(fullText) + '<span class="cursor"></span>';
                            state.tokenCount++;
                            updateStats();
                            if(state.autoScroll) requestAnimationFrame(scrollToBottom);
                        }
                    } catch(e){}
                }
            }
        }

        // Final Render (Başarılı Bitiş)
        bubbleContent.innerHTML = marked.parse(fullText);
        enhanceCodeBlocks(aiBubble);
        addMessageActions(aiBubble, fullText);
        state.history.push({role: 'assistant', content: fullText});

    } catch(err) {
        if(err.name === 'AbortError' || state.interrupted) {
            // Kasıtlı Kesilme (Interruption)
            bubbleContent.innerHTML = marked.parse(fullText) + ' <i class="fas fa-bolt" style="color:var(--warning)" title="Sözü kesildi"></i>';
            // History'e yarım da olsa ekle ki bağlam kopmasın
            if(fullText) state.history.push({role: 'assistant', content: fullText});
        } else {
            // Gerçek Hata
            bubbleContent.innerHTML += `<br><div style="color:var(--danger)">❌ Hata: ${err.message}</div>`;
            state.autoListen = false;
            stopMicUI();
        }
    } finally {
        setBusy(false);
        // Eğer eller serbestse ve kesilmediyse dinlemeye devam et
        // Eğer kesildiyse zaten 'onresult' yeni bir sendMessage tetikleyecek.
        if(state.autoListen && !state.interrupted) {
             // Küçük bir gecikme ile mikrofonun kararlı kalmasını sağla
             setTimeout(tryStartMic, 500);
        }
        if(state.autoScroll) scrollToBottom();
    }
}

// --- SPEECH RECOGNITION (Barge-In Destekli) ---
function setupSpeech() {
    if(!('webkitSpeechRecognition' in window)) { 
        $('micBtn').style.display='none'; 
        return; 
    }
    
    const rec = new webkitSpeechRecognition();
    rec.lang = 'tr-TR';
    rec.continuous = false; 
    rec.interimResults = true; 

    rec.onstart = () => { 
        $('voiceStatus').classList.remove('hidden');
        if(state.autoListen) {
            $('micBtn').classList.add('active-pulse');
            // Duruma göre metni güncelle
            if (state.generating) {
                $('voiceStatus').innerHTML = '⚡ <b>Araya Girme Aktif:</b> Dinliyor...';
                $('voiceStatus').style.color = 'var(--warning)';
            } else {
                $('voiceStatus').innerHTML = '🔴 <b>Eller Serbest:</b> Dinliyor...';
                $('voiceStatus').style.color = 'var(--danger)';
            }
        } else {
            $('micBtn').style.color = 'var(--danger)';
            $('voiceStatus').innerText = 'Dinliyor...';
        }
    };

    rec.onend = () => { 
        // Eğer autoListen aktifse ve henüz bir şey gönderilmediyse tekrar dinle
        if(state.autoListen) {
             // Eğer generating devam ediyorsa mikrofonu hemen tekrar aç (Sürekli dinle)
             // Eğer generating bittiyse sendMessage zaten tekrar açacak.
             if (state.generating) {
                 tryStartMic();
             }
        } else {
            stopMicUI();
        }
    };

    rec.onresult = (e) => {
        let final = '';
        for (let i = e.resultIndex; i < e.results.length; ++i) {
            if (e.results[i].isFinal) {
                final += e.results[i][0].transcript;
            }
        }
        
        // ARA GİRME MANTIĞI (BARGE-IN)
        if(final) {
            const val = final.trim();
            if (val.length > 0) {
                $('userInput').value = val;
                
                // Eğer AI şu an konuşuyorsa (generating), sustur!
                if (state.generating) {
                    console.log("⚡ Barge-in detected! Interrupting AI...");
                    interruptGeneration(); // Backend'i durdur
                }
                
                // Yeni mesajı gönder
                sendMessage();
            }
        }
    };

    rec.onerror = (event) => {
        if(event.error === 'no-speech' && state.autoListen) return; 
        // Aborted hatası normaldir (elle durdurunca gelir)
        if(event.error !== 'aborted') {
            console.error("Speech Error", event.error);
            state.autoListen = false;
            stopMicUI();
        }
    };

    state.recognition = rec;

    $('micBtn').onclick = () => {
        if(state.autoListen) {
            state.autoListen = false;
            interruptGeneration();
            rec.stop();
            stopMicUI();
        } else {
            rec.start();
        }
    };

    $('micBtn').ondblclick = () => {
        state.autoListen = true;
        rec.start();
    };
}

function tryStartMic() { 
    try {
        // Zaten çalışıyorsa hata verir, yakala ve geç
        if(state.recognition) state.recognition.start();
    } catch(e) {} 
}

function stopMicUI() {
    $('micBtn').style.color = '';
    $('micBtn').classList.remove('active-pulse');
    $('voiceStatus').classList.add('hidden');
}

// ... (buildPayload ve diğer UI fonksiyonları aynı kalır) ...
function buildPayload(lastMsg) {
    const msgs = [];
    const sys = $('systemPrompt').value;
    const rag = $('ragInput').value;
    let finalSystem = sys;
    if(rag) finalSystem += `\n\nBAĞLAM BİLGİSİ:\n${rag}\n\n`;
    if(finalSystem) msgs.push({role: 'system', content: finalSystem});
    const limit = parseInt($('historyLimit').value) || 10;
    state.history.slice(-limit).forEach(m => msgs.push(m));
    return {
        messages: msgs,
        temperature: parseFloat($('tempInput').value),
        max_tokens: parseInt($('tokenLimit').value),
        stream: true
    };
}

function addMessage(role, htmlContent) {
    const div = document.createElement('div');
    div.className = `message ${role}`;
    div.innerHTML = `
        <div class="avatar"><i class="fas fa-${role==='user'?'user':'robot'}"></i></div>
        <div class="bubble">
            <div class="markdown-content">${htmlContent}</div>
        </div>
    `;
    $('chatContainer').appendChild(div);
    if(state.autoScroll) scrollToBottom();
    return div.querySelector('.bubble');
}

function addMessageActions(bubble, rawText) {
    const actionsDiv = document.createElement('div');
    actionsDiv.className = 'msg-actions';
    actionsDiv.innerHTML = `
        <button class="msg-btn" onclick="copyText(this, '${encodeURIComponent(rawText)}')" title="Kopyala">
            <i class="fas fa-copy"></i>
        </button>
    `;
    bubble.appendChild(actionsDiv);
}

function enhanceCodeBlocks(element) {
    element.querySelectorAll('pre code').forEach((block) => {
        hljs.highlightElement(block);
        const pre = block.parentElement;
        const lang = block.className.replace('hljs language-', '') || 'Code';
        const header = document.createElement('div');
        header.className = 'code-header';
        header.innerHTML = `
            <span class="code-lang">${lang}</span>
            <button class="copy-code-btn" onclick="copyCode(this)">
                <i class="fas fa-copy"></i> Kopyala
            </button>
        `;
        pre.insertBefore(header, block);
    });
}

window.copyText = (btn, text) => {
    navigator.clipboard.writeText(decodeURIComponent(text));
    const icon = btn.querySelector('i');
    icon.className = 'fas fa-check';
    setTimeout(() => icon.className = 'fas fa-copy', 2000);
};

window.copyCode = (btn) => {
    const code = btn.parentElement.nextElementSibling.innerText;
    navigator.clipboard.writeText(code);
    const originalHtml = btn.innerHTML;
    btn.innerHTML = '<i class="fas fa-check"></i> Kopyalandı';
    setTimeout(() => btn.innerHTML = originalHtml, 2000);
};

function scrollToBottom() {
    const el = $('chatContainer');
    el.scrollTop = el.scrollHeight;
}

function setBusy(busy) {
    state.generating = busy;
    $('sendBtn').classList.toggle('hidden', busy);
    $('stopBtn').classList.toggle('hidden', !busy);
    
    // Barge-in modunda input açık kalmalı ki kullanıcı görebilsin
    if(!state.autoListen) {
        if(busy) $('userInput').setAttribute('disabled', true);
        else {
            $('userInput').removeAttribute('disabled');
            $('userInput').focus();
        }
    }
}

function updateStats() {
    const dur = Date.now() - state.startTime;
    $('latencyVal').innerText = `${dur}ms`;
    $('tokenVal').innerText = state.tokenCount;
    const tps = (state.tokenCount / (dur/1000)).toFixed(1);
    $('tpsVal').innerText = tps;
}

function escapeHtml(text) {
    return text.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
}

window.togglePanel = (id, open = null) => {
    const el = $(id);
    const overlay = $('overlay');
    if (open === true) el.classList.add('active');
    else if (open === false) el.classList.remove('active');
    else el.classList.toggle('active');
    const isMobile = window.innerWidth < 768;
    if(isMobile) {
        const anyActive = $('leftPanel').classList.contains('active') || $('rightPanel').classList.contains('active');
        overlay.classList.toggle('active', anyActive);
    }
};

window.closeAllPanels = () => {
    $('leftPanel').classList.remove('active');
    $('rightPanel').classList.remove('active');
    $('overlay').classList.remove('active');
};

window.switchTab = (tab) => {
    $('ragTab').style.display = tab==='rag'?'block':'none';
    $('logsTab').style.display = tab==='logs'?'block':'none';
    document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
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
    const container = $('chatContainer');
    container.innerHTML = `
        <div class="empty-state" id="emptyState" style="display: flex;">
            <div class="logo-shine"><i class="fas fa-layer-group"></i></div>
            <h2>Sentiric Engine Hazır</h2>
            <p>Parametreleri ayarla, veri yükle ve sohbete başla.</p>
        </div>
    `;
};

async function checkHealth() {
    try {
        const res = await fetch('/health');
        if(res.ok) {
            $('statusText').innerText = 'Bağlı';
            $('statusText').style.color = 'var(--success)';
            $('connStatus').querySelector('.dot').style.backgroundColor = 'var(--success)';
        }
    } catch(e) {
        $('statusText').innerText = 'Koptu';
        $('statusText').style.color = 'var(--danger)';
        $('connStatus').querySelector('.dot').style.backgroundColor = 'var(--danger)';
    }
}
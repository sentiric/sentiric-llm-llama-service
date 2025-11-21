// ==========================================
// 1. STATE & CONFIGURATION
// ==========================================
const $ = (id) => document.getElementById(id);

const state = { 
    generating: false,
    controller: null,
    history: [],
    autoListen: false,
    isRecording: false,
    recognition: null,
    startTime: 0,
    tokenCount: 0,
    autoScroll: true,
    interrupted: false,
    // YENİ: Oturum İstatistikleri
    sessionStats: {
        totalTokens: 0,
        totalTimeMs: 0,
        requestCount: 0
    }
};

// ... (Initialization ve Markdown ayarları AYNI) ...

// ==========================================
// 2. INITIALIZATION
// ==========================================
document.addEventListener('DOMContentLoaded', () => {
    const theme = localStorage.getItem('theme') || 'light';
    document.body.setAttribute('data-theme', theme);
    
    setupMarkdown();
    setupEvents();
    setupSpeech();
    checkHealth();
    setInterval(checkHealth, 10000);
    playWelcomeAnimation();
});

function setupMarkdown() {
    if (typeof marked !== 'undefined') {
        marked.setOptions({
            highlight: function(code, lang) {
                const language = hljs.getLanguage(lang) ? lang : 'plaintext';
                return hljs.highlight(code, { language }).value;
            },
            langPrefix: 'hljs language-'
        });
    }
}

// ==========================================
// 3. EVENT HANDLERS
// ==========================================
function setupEvents() {
    const input = $('userInput');
    
    input.addEventListener('input', function() {
        this.style.height = 'auto';
        this.style.height = Math.min(this.scrollHeight, 200) + 'px';
        // Yazı yazıldığında ghost text'i temizle
        $('ghostText').innerText = '';
    });

    input.addEventListener('keydown', (e) => {
        if (e.key === 'Enter' && !e.shiftKey) {
            e.preventDefault();
            if(window.innerWidth < 768) input.blur();
            if (state.generating) interruptGeneration();
            sendMessage();
        }
    });

    // ... (Diğer event handler'lar AYNI) ...
    $('sendBtn').onclick = () => {
        if(state.isRecording) stopMic(); 
        if (state.generating) interruptGeneration();
        sendMessage();
    };
    
    $('stopBtn').onclick = () => {
        state.autoListen = false;
        interruptGeneration();
        stopMic();
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


// ==========================================
// 4. CORE LOGIC
// ==========================================

function interruptGeneration() {
    if (state.controller) {
        console.log("⛔ Generation interrupted by user.");
        state.interrupted = true;
        try { state.controller.abort(); } catch(e) {}
        state.controller = null;
        setBusy(false);
    }
}

async function sendMessage() {
    const text = $('userInput').value.trim();
    if (!text) return;

    // UI Temizlik
    $('userInput').value = '';
    $('userInput').style.height = 'auto';
    $('ghostText').innerText = ''; // Ghost text temizle
    
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

    let fullText = ""; 
    const payload = buildPayload(text);

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
                            if (state.autoScroll || fullText.length % 50 === 0) {
                                bubbleContent.innerHTML = marked.parse(fullText) + '<span class="cursor"></span>';
                                if(state.autoScroll) scrollToBottom();
                            }
                            state.tokenCount++;
                            updateLiveStats(); // Anlık istatistik
                        }
                    } catch(e){}
                }
            }
        }

        // Başarılı Bitiş
        bubbleContent.innerHTML = marked.parse(fullText);
        enhanceCodeBlocks(aiBubble);
        addMessageActions(aiBubble, fullText);
        state.history.push({role: 'assistant', content: fullText});
        
        // Oturum İstatistiklerini Güncelle
        updateSessionStats(state.tokenCount, Date.now() - state.startTime);

    } catch(err) {
        if(err.name === 'AbortError' || state.interrupted) {
            console.log("✋ İstek iptal edildi.");
            const safeText = typeof fullText !== 'undefined' ? fullText : "";
            bubbleContent.innerHTML = marked.parse(safeText) + ' <span style="color:var(--warning); font-weight:bold;">[Sözü Kesildi]</span>';
            if(safeText) state.history.push({role: 'assistant', content: safeText});
        } else {
            console.error("Error:", err);
            bubbleContent.innerHTML += `<br><div style="color:var(--danger)">❌ Hata: ${err.message}</div>`;
            state.autoListen = false;
            stopMic();
        }
    } finally {
        setBusy(false);
        if(state.autoListen && !state.interrupted) {
             setTimeout(tryStartMic, 300);
        }
        if(state.autoScroll) scrollToBottom();
    }
}

// ==========================================
// 5. SPEECH RECOGNITION (ENHANCED)
// ==========================================
function setupSpeech() {
    if(!('webkitSpeechRecognition' in window)) { 
        $('micBtn').style.display='none'; 
        $('liveBtn').style.display='none';
        return; 
    }
    
    const rec = new webkitSpeechRecognition();
    rec.lang = $('langSelect').value;
    rec.continuous = false; 
    rec.interimResults = true; // YENİ: Ara sonuçları al

    rec.onstart = () => { 
        state.isRecording = true;
        updateMicUI();
        const statusEl = $('voiceStatus');
        statusEl.classList.remove('hidden');
        
        if(state.autoListen) {
            statusEl.innerHTML = state.generating 
                ? '⚡ <b>Araya Girme:</b> Dinliyor...' 
                : '🎧 <b>Canlı Mod:</b> Dinliyor...';
            statusEl.style.color = state.generating ? 'var(--warning)' : 'var(--success)';
        } else {
            statusEl.innerText = 'Dikte ediliyor...';
            statusEl.style.color = 'var(--text-sub)';
        }
    };

    rec.onend = () => { 
        state.isRecording = false;
        $('ghostText').innerText = ''; // Temizle
        if(state.autoListen) {
             setTimeout(() => {
                 if (state.autoListen && !state.isRecording) tryStartMic();
             }, 100);
        } else {
            $('voiceStatus').classList.add('hidden');
            updateMicUI();
        }
    };

    rec.onresult = (e) => {
        let final = '';
        let interim = '';

        for (let i = e.resultIndex; i < e.results.length; ++i) {
            if (e.results[i].isFinal) {
                final += e.results[i][0].transcript;
            } else {
                interim += e.results[i][0].transcript;
            }
        }

        // 1. Canlı Önizleme (Ghost Text)
        // Mevcut input değerinin üzerine interim metni hayalet gibi göster
        const currentVal = $('userInput').value;
        if (interim) {
            $('ghostText').innerText = currentVal + " " + interim + "...";
        } else {
            $('ghostText').innerText = '';
        }
        
        // 2. Final Sonuç
        if(final) {
            const val = final.trim();
            if (val.length > 0) {
                $('ghostText').innerText = ''; // Ghost'u sil
                if (!state.autoListen) {
                    const current = $('userInput').value;
                    $('userInput').value = current ? current + " " + val : val;
                } else {
                    // Barge-in & Send
                    $('userInput').value = val;
                    if (state.generating) {
                        interruptGeneration();
                        setTimeout(() => sendMessage(), 50);
                    } else {
                        sendMessage();
                    }
                }
            }
        }
    };

    // ... (Hata yönetimi ve butonlar AYNI) ...
    rec.onerror = (event) => {
        if(event.error === 'no-speech') return; 
        if(event.error !== 'aborted') {
            console.error("Speech Error:", event.error);
            if (event.error === 'network') {
                state.autoListen = false;
                stopMic();
            }
        }
    };

    state.recognition = rec;

    $('micBtn').onclick = () => {
        if(state.autoListen) {
            state.autoListen = false;
            stopMic();
            return;
        }
        if(state.isRecording) stopMic();
        else tryStartMic();
    };

    $('liveBtn').onclick = () => {
        state.autoListen = !state.autoListen;
        if(state.autoListen) {
            if(!state.isRecording) tryStartMic();
        } else {
            stopMic();
        }
        updateMicUI();
    };
    
    $('langSelect').onchange = () => {
        state.recognition.lang = $('langSelect').value;
        if(state.isRecording) { stopMic(); setTimeout(tryStartMic, 200); }
    };
}

// ... (Mic Helper fonksiyonları AYNI) ...
function tryStartMic() { 
    if(state.recognition && !state.isRecording) {
        try { state.recognition.start(); } catch(e) {}
    }
}

function stopMic() {
    if(state.recognition) {
        try { state.recognition.stop(); } catch(e) {}
    }
    state.isRecording = false;
    $('voiceStatus').classList.add('hidden');
    updateMicUI();
}

function updateMicUI() {
    const micBtn = $('micBtn');
    const liveBtn = $('liveBtn');
    micBtn.style.color = '';
    micBtn.classList.remove('active-pulse');
    liveBtn.style.color = '';
    liveBtn.classList.remove('active-pulse');
    micBtn.style.opacity = '1';
    if (state.autoListen) {
        liveBtn.style.color = 'white';
        liveBtn.classList.add('active-pulse');
        micBtn.style.opacity = '0.4';
    } else if (state.isRecording) {
        micBtn.style.color = 'var(--danger)';
    }
}


// ==========================================
// 6. STATISTICS & DATA MANAGEMENT (NEW)
// ==========================================

function updateLiveStats() {
    const dur = Date.now() - state.startTime;
    $('latencyVal').innerText = `${dur}ms`;
    const tps = (state.tokenCount / (dur/1000)).toFixed(1);
    $('tpsVal').innerText = tps;
}

function updateSessionStats(tokens, durationMs) {
    state.sessionStats.totalTokens += tokens;
    state.sessionStats.totalTimeMs += durationMs;
    state.sessionStats.requestCount++;
    
    $('sessionTotalTokenVal').innerText = state.sessionStats.totalTokens;
    
    if (state.sessionStats.totalTimeMs > 0) {
        const avgTps = (state.sessionStats.totalTokens / (state.sessionStats.totalTimeMs / 1000)).toFixed(1);
        $('sessionAvgTpsVal').innerText = avgTps;
    }
}

// YENİ: Geçmişi İndir
window.downloadHistory = () => {
    if (state.history.length === 0) {
        alert("İndirilecek geçmiş yok.");
        return;
    }
    
    const dataStr = "data:text/json;charset=utf-8," + encodeURIComponent(JSON.stringify(state.history, null, 2));
    const downloadAnchorNode = document.createElement('a');
    downloadAnchorNode.setAttribute("href", dataStr);
    downloadAnchorNode.setAttribute("download", "sentiric_chat_history_" + new Date().toISOString() + ".json");
    document.body.appendChild(downloadAnchorNode);
    downloadAnchorNode.click();
    downloadAnchorNode.remove();
};

// ... (Geri kalan UI Helper fonksiyonları: buildPayload, addMessage, setBusy vs. AYNI) ...

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
        if (!pre.querySelector('.code-header')) {
            const header = document.createElement('div');
            header.className = 'code-header';
            header.innerHTML = `
                <span class="code-lang">${lang}</span>
                <button class="copy-code-btn" onclick="copyCode(this)">
                    <i class="fas fa-copy"></i> Kopyala
                </button>
            `;
            pre.insertBefore(header, block);
        }
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
    
    if(!state.autoListen) {
        if(busy) $('userInput').setAttribute('disabled', true);
        else {
            $('userInput').removeAttribute('disabled');
            $('userInput').focus();
        }
    }
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
    if(window.innerWidth < 768) {
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
    state.sessionStats = { totalTokens: 0, totalTimeMs: 0, requestCount: 0 };
    $('sessionTotalTokenVal').innerText = '0';
    $('sessionAvgTpsVal').innerText = '--';
    
    const container = $('chatContainer');
    container.innerHTML = `
        <div class="empty-state" id="emptyState" style="display: flex;">
            <div class="logo-shine"><i class="fas fa-layer-group"></i></div>
            <h2>Sentiric Engine Hazır</h2>
            <p>Parametreleri ayarla, veri yükle ve sohbete başla.</p>
        </div>
    `;
};

async function playWelcomeAnimation() {
    $('chatContainer').innerHTML = '';
    const welcomeText = `### 🚀 Sentiric Omni-Studio Hazır!

Ben sizin **Yerel, Özel ve Hızlı** yapay zeka motorunuzum.

**🎛️ Nasıl Kullanılır?**
*   🎤 **Dikte:** Mesajınızı yazdırmak için mikrofona bir kez basın.
*   🎧 **Canlı Mod (Barge-in):** Kulaklık ikonuna basın. Ben konuşurken bile sözümü kesebilirsiniz, sizi sürekli dinlerim.
*   📂 **RAG (Veri):** Dokümanlarınızı sürükleyip bırakarak veya ataş ikonuna basarak hafızama ekleyebilirsiniz.

**⚡ Sistem Durumu:**
*   **Motor:** Sentirik 1B (GPU Accelerated)
*   **Hafıza:** Akıllı Context Yönetimi (8k)

*Hadi başlayalım! Ne hakkında konuşmak istersiniz?*`;

    const div = document.createElement('div');
    div.className = 'message ai';
    div.innerHTML = `
        <div class="avatar"><i class="fas fa-cube"></i></div>
        <div class="bubble"><div class="markdown-content"></div></div>
    `;
    $('chatContainer').appendChild(div);
    
    const contentDiv = div.querySelector('.markdown-content');
    let i = 0;
    const speed = 10; 
    
    function type() {
        if (i < welcomeText.length) {
            const currentText = welcomeText.substring(0, i + 1);
            contentDiv.innerHTML = marked.parse(currentText) + '<span class="cursor"></span>';
            i++;
            scrollToBottom();
            setTimeout(type, speed);
        } else {
            contentDiv.innerHTML = marked.parse(welcomeText);
            enhanceCodeBlocks(div);
            addQuickReplies(div);
        }
    }
    type();
    state.history.push({role: 'assistant', content: welcomeText});
}

function addQuickReplies(bubbleDiv) {
    const chips = document.createElement('div');
    chips.className = 'quick-replies';
    chips.innerHTML = `
        <button onclick="$('userInput').value='Bana bir şiir yaz'; sendMessage()">📝 Şiir Yaz</button>
        <button onclick="$('userInput').value='Bu sistemi kim yaptı?'; sendMessage()">🤔 Kimsin?</button>
        <button onclick="$('userInput').value='RAG sistemi nasıl çalışır?'; sendMessage()">📂 RAG Nedir?</button>
    `;
    bubbleDiv.querySelector('.bubble').appendChild(chips);
}

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
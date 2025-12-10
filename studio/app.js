/**
 * SENTIRIC OMNI-STUDIO v3.2 (Profile Manager)
 */

const $ = (id) => document.getElementById(id);

const state = { 
    generating: false,
    controller: null,
    history: [],
    tokenCount: 0,
    startTime: 0,
    interrupted: false,
    autoScroll: true,
    isRecording: false,
    autoListen: false,
    recognition: null,
    audioContext: null,
    analyser: null,
    microphone: null,
    visualizerFrame: null,
    sessionStats: { totalTokens: 0, totalTimeMs: 0, requestCount: 0 },
    // YENİ: Model State
    currentProfile: "",
    isSwitchingModel: false
};

document.addEventListener('DOMContentLoaded', () => {
    const theme = localStorage.getItem('theme') || 'light';
    document.body.setAttribute('data-theme', theme);
    
    setupMarkdown();
    setupEvents();
    setupSpeech();
    
    // Önce modelleri çek, sonra health check yap
    fetchModels().then(() => {
        checkHealth();
        setInterval(checkHealth, 5000);
    });
});

// ==========================================
// MODEL MANAGEMENT (YENİ)
// ==========================================

async function fetchModels() {
    try {
        const res = await fetch('/v1/models');
        if (!res.ok) throw new Error("Model listesi alınamadı");
        
        const json = await res.json();
        const selector = $('modelSelector');
        selector.innerHTML = ''; // Temizle

        let activeProfile = "";

        // Profil isimlerine göre (sentiric_...) filtrele veya hepsini göster
        json.data.forEach(model => {
            const option = document.createElement('option');
            // ID yerine profil adını kullanıyoruz (UI için daha temiz)
            option.value = model.profile; 
            
            // Kullanıcı dostu isim oluşturma
            let displayName = model.profile
                .replace('sentiric_', '')
                .replace(/_/g, ' ')
                .toUpperCase();
            
            // Eğer açıklama varsa (profiles.json'dan geliyorsa) onu da kullanabiliriz ama 
            // buradaki /v1/models response'u şu an sadece temel bilgileri dönüyor.
            // Backend'i description dönecek şekilde güncellemek iyi olurdu ama
            // şimdilik profil isminden yürüyoruz.
            
            option.text = `${displayName}`;
            
            if (model.active) {
                option.selected = true;
                activeProfile = model.profile;
            }
            selector.appendChild(option);
        });

        state.currentProfile = activeProfile;
        
        // Event Listener
        selector.onchange = async (e) => {
            const newProfile = e.target.value;
            if (newProfile !== state.currentProfile) {
                await switchModel(newProfile);
            }
        };

    } catch (e) {
        console.error("Model fetch error:", e);
        $('modelSelector').innerHTML = '<option>Bağlantı Hatası</option>';
    }
}

async function switchModel(profileName) {
    if (state.isSwitchingModel) return;
    
    const selector = $('modelSelector');
    const prevIndex = selector.selectedIndex;
    
    if (!confirm(`"${profileName}" profiline geçiş yapılsın mı?\n(Model indirilmemişse indirme süresi eklenecektir.)`)) {
        // Eski seçime geri dön
        selector.value = state.currentProfile;
        return;
    }

    state.isSwitchingModel = true;
    selector.disabled = true;
    $('statusText').innerText = 'Model Yükleniyor...';
    $('statusText').style.color = 'var(--warning)';
    $('connStatus').querySelector('.dot').style.backgroundColor = 'var(--warning)';
    
    addMessage('ai', `🔄 **Sistem:** Model değiştiriliyor: \`${profileName}\`... Lütfen bekleyin.`);

    try {
        const res = await fetch('/v1/models/switch', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({ profile: profileName })
        });

        const data = await res.json();

        if (res.ok && data.status === 'success') {
            state.currentProfile = profileName;
            addMessage('ai', `✅ **Sistem:** Başarıyla geçiş yapıldı: \`${profileName}\`. Hazırım!`);
            $('activeModelTag').innerText = profileName.includes('speed') ? 'FAST' : 'SMART';
        } else {
            throw new Error(data.message || "Bilinmeyen hata");
        }
    } catch (e) {
        addMessage('ai', `❌ **Hata:** Model değiştirilemedi: ${e.message}`);
        // UI'ı eski haline getir
        selector.value = state.currentProfile;
    } finally {
        state.isSwitchingModel = false;
        selector.disabled = false;
        checkHealth(); // Durumu güncelle
    }
}

// ... (setupMarkdown, setupEvents, setupSpeech, sendMessage vb. mevcut fonksiyonlar AYNI kalacak) ...
// (Buraya app.js'in geri kalanını kopyalayın, değişiklik yok)

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

function setupEvents() {
    const input = $('userInput');
    input.addEventListener('input', function() {
        this.style.height = 'auto';
        this.style.height = Math.min(this.scrollHeight, 200) + 'px';
        if($('ghostText')) $('ghostText').innerText = '';
    });
    input.addEventListener('keydown', (e) => {
        if (e.key === 'Enter' && !e.shiftKey) {
            e.preventDefault();
            if(window.innerWidth < 768) input.blur();
            if (state.generating) interruptGeneration();
            sendMessage();
        }
    });
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

function interruptGeneration() {
    if (state.controller) {
        console.log("⛔ Generation interrupted.");
        state.interrupted = true;
        try { state.controller.abort(); } catch(e) {}
        state.controller = null;
        setBusy(false);
        window.speechSynthesis.cancel();
    }
}

async function sendMessage() {
    const text = $('userInput').value.trim();
    if (!text) return;

    window.speechSynthesis.cancel();
    $('userInput').value = '';
    $('userInput').style.height = 'auto';
    if($('ghostText')) $('ghostText').innerText = '';
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
    
    if($('payloadLog')) $('payloadLog').innerText = JSON.stringify(payload, null, 2);

    if (state.autoListen) tryStartMic();

    try {
        const response = await fetch('/v1/chat/completions', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify(payload),
            signal: state.controller.signal
        });

        if (!response.ok) {
            // Hata durumunda (örn: model loading)
            if (response.status === 503) throw new Error("Model yükleniyor, lütfen bekleyin...");
            throw new Error(await response.text());
        }

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
                            updateLiveStats();
                        }
                    } catch(e){}
                }
            }
        }

        bubbleContent.innerHTML = marked.parse(fullText);
        enhanceCodeBlocks(aiBubble);
        addMessageActions(aiBubble, fullText);
        state.history.push({role: 'assistant', content: fullText});
        updateSessionStats(state.tokenCount, Date.now() - state.startTime);

        if ($('ttsToggle') && $('ttsToggle').checked && fullText) {
            speakText(fullText);
        }

    } catch(err) {
        if(err.name === 'AbortError' || state.interrupted) {
            const safeText = typeof fullText !== 'undefined' ? fullText : "";
            bubbleContent.innerHTML = marked.parse(safeText) + ' <span style="color:var(--warning); font-weight:bold;">[Sözü Kesildi]</span>';
            if(safeText) state.history.push({role: 'assistant', content: safeText});
        } else {
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

function setupSpeech() {
    if(!('webkitSpeechRecognition' in window)) { 
        console.warn("Web Speech API not supported.");
        if($('micBtn')) $('micBtn').style.display='none'; 
        if($('liveBtn')) $('liveBtn').style.display='none';
        return; 
    }
    const rec = new webkitSpeechRecognition();
    rec.lang = $('langSelect').value;
    rec.continuous = false;     
    rec.interimResults = true;  

    rec.onstart = () => { 
        state.isRecording = true;
        updateMicUI();
        const statusEl = $('voiceStatus');
        statusEl.classList.remove('hidden');
        if(state.autoListen) {
            statusEl.innerHTML = state.generating ? '⚡ <b>Araya Girme:</b> Dinliyor...' : '🎧 <b>Canlı Mod:</b> Dinliyor...';
            statusEl.style.color = state.generating ? 'var(--warning)' : 'var(--success)';
        } else {
            statusEl.innerHTML = '🎙️ Dikte ediliyor...';
            statusEl.style.color = 'var(--text-sub)';
        }
        startAudioVisualizer();
    };

    rec.onend = () => { 
        state.isRecording = false;
        if($('ghostText')) $('ghostText').innerText = '';
        stopAudioVisualizer();
        if(state.autoListen) {
             setTimeout(() => { if (state.autoListen && !state.isRecording) tryStartMic(); }, 100);
        } else {
            $('voiceStatus').classList.add('hidden');
            updateMicUI();
        }
    };

    rec.onresult = (e) => {
        let final = '';
        let interim = '';
        for (let i = e.resultIndex; i < e.results.length; ++i) {
            if (e.results[i].isFinal) final += e.results[i][0].transcript;
            else interim += e.results[i][0].transcript;
        }
        const currentVal = $('userInput').value;
        if (interim && $('ghostText')) {
            $('ghostText').innerText = currentVal + " " + interim + "...";
        } else if ($('ghostText')) {
            $('ghostText').innerText = '';
        }
        if(final) {
            const val = final.trim();
            if (val.length > 0) {
                if($('ghostText')) $('ghostText').innerText = '';
                if (!state.autoListen) {
                    const current = $('userInput').value;
                    $('userInput').value = current ? current + " " + val : val;
                } else {
                    $('userInput').value = val;
                    if (state.generating) {
                        console.log("⚡ Barge-in Triggered!");
                        interruptGeneration();
                        setTimeout(() => sendMessage(), 50);
                    } else {
                        sendMessage();
                    }
                }
            }
        }
    };
    rec.onerror = (event) => {
        if(event.error === 'no-speech') return; 
        if(event.error !== 'aborted') {
            if (event.error === 'network') { state.autoListen = false; stopMic(); }
        }
    };
    state.recognition = rec;
    $('micBtn').onclick = () => {
        if(state.autoListen) { state.autoListen = false; stopMic(); return; }
        if(state.isRecording) stopMic(); else tryStartMic();
    };
    $('liveBtn').onclick = () => {
        state.autoListen = !state.autoListen;
        if(state.autoListen) { if(!state.isRecording) tryStartMic(); } else { stopMic(); }
        updateMicUI();
    };
    $('langSelect').onchange = () => {
        state.recognition.lang = $('langSelect').value;
        if(state.isRecording) { stopMic(); setTimeout(tryStartMic, 200); }
    };
}

function tryStartMic() { 
    if(state.recognition && !state.isRecording) { try { state.recognition.start(); } catch(e) {} }
}
function stopMic() {
    if(state.recognition) { try { state.recognition.stop(); } catch(e) {} }
    state.isRecording = false;
    $('voiceStatus').classList.add('hidden');
    updateMicUI();
}
function updateMicUI() {
    const micBtn = $('micBtn');
    const liveBtn = $('liveBtn');
    if(!micBtn || !liveBtn) return;
    micBtn.style.color = ''; micBtn.classList.remove('active-pulse');
    liveBtn.style.color = ''; liveBtn.classList.remove('active-pulse');
    micBtn.style.opacity = '1';
    if (state.autoListen) {
        liveBtn.style.color = 'white'; liveBtn.classList.add('active-pulse'); micBtn.style.opacity = '0.4';
    } else if (state.isRecording) {
        micBtn.style.color = 'var(--danger)';
    }
}

async function startAudioVisualizer() {
    const canvas = $('audioVisualizer');
    if(!canvas) return;
    canvas.classList.add('active');
    try {
        if (!state.audioContext) { state.audioContext = new (window.AudioContext || window.webkitAudioContext)(); }
        if (state.audioContext.state === 'suspended') await state.audioContext.resume();
        const stream = await navigator.mediaDevices.getUserMedia({ audio: true });
        if (state.microphone) state.microphone.disconnect();
        if (state.analyser) state.analyser.disconnect();
        state.microphone = state.audioContext.createMediaStreamSource(stream);
        state.analyser = state.audioContext.createAnalyser();
        state.analyser.fftSize = 256;
        state.microphone.connect(state.analyser);
        drawVisualizer();
    } catch (e) { console.warn("Visualizer failed:", e); }
}
function stopAudioVisualizer() {
    const canvas = $('audioVisualizer');
    if(canvas) canvas.classList.remove('active');
    if(state.visualizerFrame) cancelAnimationFrame(state.visualizerFrame);
}
function drawVisualizer() {
    const canvas = $('audioVisualizer');
    if(!canvas) return;
    const ctx = canvas.getContext('2d');
    const bufferLength = state.analyser.frequencyBinCount;
    const dataArray = new Uint8Array(bufferLength);
    const draw = () => {
        state.visualizerFrame = requestAnimationFrame(draw);
        state.analyser.getByteFrequencyData(dataArray);
        ctx.fillStyle = getComputedStyle(document.body).getPropertyValue('--bg-sidebar').trim();
        ctx.fillRect(0, 0, canvas.width, canvas.height);
        const barWidth = (canvas.width / bufferLength) * 2.5;
        let barHeight; let x = 0;
        const primaryColor = getComputedStyle(document.body).getPropertyValue('--primary').trim();
        ctx.fillStyle = primaryColor;
        for(let i = 0; i < bufferLength; i++) {
            barHeight = dataArray[i] / 2;
            ctx.fillRect(x, (canvas.height - barHeight) / 2, barWidth, barHeight);
            x += barWidth + 1;
        }
    };
    draw();
}

function speakText(text) {
    if (!window.speechSynthesis) return;
    const cleanText = text.replace(/[#*`_]/g, '');
    const utterance = new SpeechSynthesisUtterance(cleanText);
    utterance.lang = $('langSelect').value;
    utterance.rate = 1.0;
    window.speechSynthesis.speak(utterance);
}

function updateLiveStats() {
    const dur = Date.now() - state.startTime;
    if($('latencyVal')) $('latencyVal').innerText = `${dur}ms`;
    const tps = (state.tokenCount / (dur/1000)).toFixed(1);
    if($('tpsVal')) $('tpsVal').innerText = tps;
}
function updateSessionStats(tokens, durationMs) {
    state.sessionStats.totalTokens += tokens;
    state.sessionStats.totalTimeMs += durationMs;
    state.sessionStats.requestCount++;
    const tokenEl = $('sessionTotalTokenVal');
    if(tokenEl) tokenEl.innerText = state.sessionStats.totalTokens;
    if (state.sessionStats.totalTimeMs > 0) {
        const avgTps = (state.sessionStats.totalTokens / (state.sessionStats.totalTimeMs / 1000)).toFixed(1);
        const avgTpsEl = $('sessionAvgTpsVal');
        if(avgTpsEl) avgTpsEl.innerText = avgTps;
    }
}

window.downloadHistory = () => {
    if (state.history.length === 0) { alert("İndirilecek geçmiş yok."); return; }
    const dataStr = "data:text/json;charset=utf-8," + encodeURIComponent(JSON.stringify(state.history, null, 2));
    const node = document.createElement('a');
    node.setAttribute("href", dataStr);
    node.setAttribute("download", "sentiric_chat_" + new Date().toISOString() + ".json");
    document.body.appendChild(node);
    node.click();
    node.remove();
};

function buildPayload(text) {
    const msgs = [];
    const sys = $('systemPrompt').value;
    const rag = $('ragInput').value;
    let finalSystem = sys;
    if(rag) finalSystem += `\n\nBAĞLAM BİLGİSİ:\n${rag}\n\n`;
    if(finalSystem) msgs.push({role: 'system', content: finalSystem});
    const limit = parseInt($('historyLimit').value) || 10;
    state.history.slice(-limit).forEach(m => msgs.push(m));
    return { messages: msgs, temperature: parseFloat($('tempInput').value), max_tokens: parseInt($('tokenLimit').value), stream: true };
}

function addMessage(role, htmlContent) {
    const div = document.createElement('div');
    div.className = `message ${role}`;
    div.innerHTML = `<div class="avatar"><i class="fas fa-${role==='user'?'user':'robot'}"></i></div><div class="${role}">${role}</div><div class="bubble"><div class="markdown-content">${htmlContent}</div></div>`;
    $('chatContainer').appendChild(div);
    if(state.autoScroll) scrollToBottom();
    return div.querySelector('.bubble');
}

function addMessageActions(bubble, rawText) {
    const actionsDiv = document.createElement('div');
    actionsDiv.className = 'msg-actions';
    actionsDiv.innerHTML = `<button class="msg-btn" onclick="copyText(this, '${encodeURIComponent(rawText)}')" title="Kopyala"><i class="fas fa-copy"></i></button>`;
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
            header.innerHTML = `<span class="code-lang">${lang}</span><button class="copy-code-btn" onclick="copyCode(this)"><i class="fas fa-copy"></i> Kopyala</button>`;
            pre.insertBefore(header, block);
        }
    });
}

window.copyText = (btn, text) => {
    navigator.clipboard.writeText(decodeURIComponent(text));
    const icon = btn.querySelector('i'); icon.className = 'fas fa-check'; setTimeout(() => icon.className = 'fas fa-copy', 2000);
};
window.copyCode = (btn) => {
    const code = btn.parentElement.nextElementSibling.innerText;
    navigator.clipboard.writeText(code);
    const originalHtml = btn.innerHTML; btn.innerHTML = '<i class="fas fa-check"></i> Kopyalandı'; setTimeout(() => btn.innerHTML = originalHtml, 2000);
};
function scrollToBottom() { const el = $('chatContainer'); el.scrollTop = el.scrollHeight; }
function setBusy(busy) {
    state.generating = busy;
    $('sendBtn').classList.toggle('hidden', busy);
    $('stopBtn').classList.toggle('hidden', !busy);
    if(!state.autoListen) { if(busy) $('userInput').setAttribute('disabled', true); else { $('userInput').removeAttribute('disabled'); $('userInput').focus(); } }
}
function escapeHtml(text) { return text.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;"); }
window.togglePanel = (id, open = null) => {
    const el = $(id); const overlay = $('overlay');
    if (open === true) el.classList.add('active'); else if (open === false) el.classList.remove('active'); else el.classList.toggle('active');
    if(window.innerWidth < 768) { const anyActive = $('leftPanel').classList.contains('active') || $('rightPanel').classList.contains('active'); overlay.classList.toggle('active', anyActive); }
};
window.closeAllPanels = () => { $('leftPanel').classList.remove('active'); $('rightPanel').classList.remove('active'); $('overlay').classList.remove('active'); };
window.switchTab = (tab) => {
    $('ragTab').style.display = tab==='rag'?'block':'none';
    $('logsTab').style.display = tab==='logs'?'block':'none';
    document.querySelectorAll('.tab').forEach(t => t.classList.remove('active')); event.target.classList.add('active');
};
window.toggleTheme = () => {
    const current = document.body.getAttribute('data-theme'); const next = current === 'light' ? 'dark' : 'light';
    document.body.setAttribute('data-theme', next); localStorage.setItem('theme', next);
};
window.clearChat = () => {
    state.history = []; state.sessionStats = { totalTokens: 0, totalTimeMs: 0, requestCount: 0 };
    if($('sessionTotalTokenVal')) $('sessionTotalTokenVal').innerText = '0';
    if($('sessionAvgTpsVal')) $('sessionAvgTpsVal').innerText = '--';
    $('chatContainer').innerHTML = `<div class="empty-state" id="emptyState"><div class="logo-shine"><i class="fas fa-layer-group"></i></div><h2>Sentiric Engine Hazır</h2><p>Profil seçin, mikrofonu açın veya yazmaya başlayın.</p></div>`;
};
async function checkHealth() {
    try {
        const res = await fetch('/health');
        if(res.ok) {
            $('statusText').innerText = 'Bağlı'; $('statusText').style.color = 'var(--success)';
            $('connStatus').querySelector('.dot').style.backgroundColor = 'var(--success)';
        }
    } catch(e) {
        $('statusText').innerText = 'Koptu'; $('statusText').style.color = 'var(--danger)';
        $('connStatus').querySelector('.dot').style.backgroundColor = 'var(--danger)';
    }
}
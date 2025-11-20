// GLOBAL STATE
const state = {
    isGenerating: false,
    controller: null,
    startTime: 0,
    tokenCount: 0,
    theme: localStorage.getItem('theme') || 'light'
};

// DOM REFERENCES
const els = {
    userInput: document.getElementById('userInput'),
    chatMessages: document.getElementById('chatMessages'),
    sendBtn: document.getElementById('sendBtn'),
    stopBtn: document.getElementById('stopBtn'),
    systemPrompt: document.getElementById('systemPrompt'),
    ragInput: document.getElementById('ragInput'),
    // Params
    tempInput: document.getElementById('tempInput'),
    tempVal: document.getElementById('tempVal'),
    topPInput: document.getElementById('topPInput'),
    topPVal: document.getElementById('topPVal'),
    repPenInput: document.getElementById('repPenInput'),
    repPenVal: document.getElementById('repPenVal'),
    seedInput: document.getElementById('seedInput'),
    maxTokensInput: document.getElementById('maxTokensInput'),
    // Metrics
    statusDot: document.querySelector('.status-dot'),
    statusText: document.getElementById('statusText'),
    lastLatency: document.getElementById('lastLatency'),
    lastTPS: document.getElementById('lastTPS'),
    consoleLog: document.getElementById('consoleLog'),
    promptTokenVal: document.getElementById('promptTokenVal'),
    genTokenVal: document.getElementById('genTokenVal')
};

// INIT
document.addEventListener('DOMContentLoaded', () => {
    applyTheme(state.theme);
    loadHistory();
    
    // Event Listeners
    setupInputHandlers();
    setupParamListeners();
    
    checkHealth();
    setInterval(checkHealth, 8000);

    // Default View
    setViewInternal('focus');
});

// --- HANDLERS ---

function setupInputHandlers() {
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
    if(els.sendBtn) els.sendBtn.addEventListener('click', sendMessage);
    if(els.stopBtn) els.stopBtn.addEventListener('click', stopGeneration);
}

function setupParamListeners() {
    const bindSlider = (input, display) => {
        if(input && display) {
            input.addEventListener('input', (e) => display.textContent = e.target.value);
        }
    };
    bindSlider(els.tempInput, els.tempVal);
    bindSlider(els.topPInput, els.topPVal);
    bindSlider(els.repPenInput, els.repPenVal);
}

// --- CORE LOGIC ---

async function sendMessage() {
    const text = els.userInput.value.trim();
    if (!text || state.isGenerating) return;

    // UI Updates
    els.userInput.value = '';
    els.userInput.style.height = 'auto';
    document.querySelector('.welcome-screen').style.display = 'none';

    appendMessage('user', text);
    const aiMsgContent = appendMessage('ai', '<span class="cursor"></span>');
    
    setGeneratingState(true);
    state.controller = new AbortController();
    state.startTime = Date.now();
    state.tokenCount = 0;
    
    let fullResponse = "";
    
    // Build Request Body (Advanced Params)
    const requestBody = {
        messages: buildMessages(text),
        temperature: parseFloat(els.tempInput.value),
        max_tokens: parseInt(els.maxTokensInput.value),
        top_p: parseFloat(els.topPInput.value),
        repeat_penalty: parseFloat(els.repPenInput.value),
        seed: parseInt(els.seedInput.value),
        stream: true
    };

    logToConsole(`Request: ${JSON.stringify(requestBody.messages.length)} msgs, Temp: ${requestBody.temperature}`);

    try {
        const response = await fetch('/v1/chat/completions', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(requestBody),
            signal: state.controller.signal
        });

        if (!response.ok) throw new Error(`HTTP ${response.status}`);

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
                            // Scroll only if near bottom
                            scrollToBottom();
                        }
                    } catch (e) {}
                }
            }
        }

        // Final Polish
        finishGeneration(aiMsgContent, fullResponse);
        saveHistory(text, fullResponse);

    } catch (err) {
        if (err.name !== 'AbortError') {
            aiMsgContent.innerHTML += `<br><span style="color:#ef4444">[Error: ${err.message}]</span>`;
            logToConsole(`Error: ${err.message}`);
        } else {
            aiMsgContent.innerHTML += " <span style='color:#888'>[Durduruldu]</span>";
        }
    } finally {
        setGeneratingState(false);
        state.controller = null;
    }
}

function buildMessages(userText) {
    const msgs = [];
    if (els.systemPrompt.value.trim()) {
        msgs.push({ role: "system", content: els.systemPrompt.value });
    }
    const rag = els.ragInput.value.trim();
    const content = rag ? `BAĞLAM:\n${rag}\n\nSORU:\n${userText}` : userText;
    msgs.push({ role: "user", content: content });
    return msgs;
}

function finishGeneration(element, fullText) {
    element.innerHTML = marked.parse(fullText);
    
    // Syntax Highlight
    element.querySelectorAll('pre code').forEach((block) => {
        hljs.highlightElement(block);
        addCopyButton(block);
    });

    // Metrics
    const duration = Date.now() - state.startTime;
    const seconds = duration / 1000;
    const tps = state.tokenCount / seconds;
    
    els.lastLatency.textContent = `${duration}ms`;
    els.lastTPS.textContent = `${tps.toFixed(1)} t/s`;
    
    // Mock Token Counts (Backend doesn't send usage in stream yet, normally)
    els.genTokenVal.textContent = state.tokenCount;
    logToConsole(`Finished: ${state.tokenCount} tokens in ${seconds.toFixed(2)}s`);
}

// --- UTILITIES ---

function addCopyButton(block) {
    const pre = block.parentElement;
    const btn = document.createElement('button');
    btn.className = 'copy-btn';
    btn.innerHTML = '<i class="fas fa-copy"></i>';
    btn.onclick = () => {
        navigator.clipboard.writeText(block.innerText);
        btn.innerHTML = '<i class="fas fa-check"></i>';
        setTimeout(() => btn.innerHTML = '<i class="fas fa-copy"></i>', 2000);
    };
    pre.appendChild(btn);
}

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

function forceJSONMode() {
    els.systemPrompt.value = "Sen bir JSON API'sisin. Sadece geçerli JSON formatında yanıt ver. Açıklama yapma.";
    alert("System Prompt JSON modu için güncellendi.");
}

function clearChat() {
    els.chatMessages.innerHTML = `
        <div class="welcome-screen">
            <div class="welcome-icon"><i class="fas fa-eraser"></i></div>
            <h2>Sohbet Temizlendi</h2>
        </div>`;
    localStorage.removeItem('chatHistory');
    logToConsole("Chat history cleared.");
}

function logToConsole(msg) {
    const div = document.createElement('div');
    div.className = 'log-entry';
    div.textContent = `[${new Date().toLocaleTimeString()}] ${msg}`;
    els.consoleLog.prepend(div);
}

function setGeneratingState(active) {
    state.isGenerating = active;
    els.sendBtn.classList.toggle('hidden', active);
    els.stopBtn.classList.toggle('hidden', !active);
}

function scrollToBottom() {
    els.chatMessages.scrollTop = els.chatMessages.scrollHeight;
}

// --- HISTORY & THEME ---

function saveHistory(user, ai) {
    let history = JSON.parse(localStorage.getItem('chatHistory') || '[]');
    history.push({ user, ai });
    if (history.length > 20) history.shift(); // Keep last 20
    localStorage.setItem('chatHistory', JSON.stringify(history));
}

function loadHistory() {
    const history = JSON.parse(localStorage.getItem('chatHistory') || '[]');
    if (history.length > 0) {
        document.querySelector('.welcome-screen').style.display = 'none';
        history.forEach(item => {
            appendMessage('user', item.user);
            const aiDiv = appendMessage('ai', '');
            finishGeneration(aiDiv, item.ai);
        });
        logToConsole("Session history loaded.");
    }
}

window.switchTheme = function() {
    state.theme = state.theme === 'light' ? 'dark' : 'light';
    applyTheme(state.theme);
    localStorage.setItem('theme', state.theme);
};

function applyTheme(theme) {
    if (theme === 'dark') document.body.setAttribute('data-theme', 'dark');
    else document.body.removeAttribute('data-theme');
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

// --- VIEW LOGIC ---
function setViewInternal(mode) {
    const left = document.getElementById('leftPanel');
    const right = document.getElementById('rightPanel');
    const buttons = document.querySelectorAll('.view-btn');

    buttons.forEach(b => b.classList.remove('active'));
    // Basit toggle
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
window.togglePanel = (id) => {
    const el = document.getElementById(id);
    el.style.display = el.style.display === 'none' ? 'flex' : 'none';
};
window.switchTab = (tab) => {
    document.querySelectorAll('.tab-content').forEach(el => el.style.display = 'none');
    document.getElementById(tab + 'Tab').style.display = 'block';
    document.querySelectorAll('.context-tab').forEach(el => el.classList.remove('active'));
    // Event target fix via simple loop or specialized selector if needed, but simple click works
    if(event) event.target.classList.add('active');
};
function stopGeneration() { if(state.controller) state.controller.abort(); }
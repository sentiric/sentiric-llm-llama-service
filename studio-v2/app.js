/**
 * SENTIRIC AGENT OS v6.2 (STABLE) - Dynamic UI PoC
 */

const $ = (id) => document.getElementById(id);

const Store = {
    generating: false,
    controller: null,
    history: [],
    startTime: 0,
    tokenCount: 0,
    currentProfile: "sentiric_eco_multitask",
    isSwitching: false,
    chart: null,
    chartData: { labels: Array(30).fill(''), data: Array(30).fill(0) }
};

const MODEL_CATALOG = [
    { id: "sentiric_eco_multitask", name: "ECO MULTITASK", desc: "Llama 3.2 3B • Hızlı", icon: "fas fa-bolt" },
    { id: "sentiric_tr_balanced", name: "TR BALANCED", desc: "Llama 3.1 8B • Dengeli", icon: "fas fa-brain" },
    { id: "sentiric_coder_eco", name: "CODER ECO", desc: "Qwen 2.5 7B • Kodlama", icon: "fas fa-code" },
    { id: "sentiric_prod_performance", name: "PROD PERF", desc: "Llama 3.1 8B • Full", icon: "fas fa-rocket" }
];

const PERSONAS = {
    'default': "Sen yardımsever bir asistansın. Türkçe konuş.",
    'coder': "Sen uzman bir yazılımcısın. Kod örnekleri ver.",
    'creative': "Sen yaratıcı bir yazarsın.",
    'english': "You are an English assistant."
};

document.addEventListener('DOMContentLoaded', () => {
    console.log("🌌 Sentiric OS v6.2 Booting (Dynamic UI PoC)...");
    
    renderModelList();
    renderDynamicControls(); // YENİ FONKSİYON
    setupEvents();
    setupCharts();
    setPersona('default');
    checkHealth();
    setInterval(checkHealth, 5000);
});

// --- YENİ BÖLÜM: DINAMIK UI OLUŞTURMA ---

async function renderDynamicControls() {
    try {
        const res = await fetch('/v1/ui/layout');
        if (!res.ok) throw new Error('Layout schema could not be fetched.');
        const schema = await res.json();

        const settingsContainer = $('dynamic-controls');
        const telemetryContainer = $('dynamic-telemetry-controls');
        if (!settingsContainer || !telemetryContainer) return;
        
        let settingsHTML = '';
        let telemetryHTML = '';

        schema.widgets.forEach(widget => {
            let html = '';
            switch(widget.type) {
                case 'textarea':
                    html = `
                        <div class="setting-group">
                            <label>${widget.label}</label>
                            <textarea id="${widget.id}" class="code-area" placeholder="${widget.properties.placeholder || ''}" rows="${widget.properties.rows || 3}"></textarea>
                        </div>`;
                    // RAG context'i Telemetry paneline de koyalım
                    telemetryHTML += html;
                    break;
                case 'slider':
                    html = `
                        <div class="setting-group">
                            <label>${widget.label.toUpperCase()}</label>
                            <div class="range-wrap">
                                <div class="range-head"><span>${widget.label}</span><span id="${widget.display_id}">${widget.properties.value}</span></div>
                                <input type="range" id="${widget.id}" min="${widget.properties.min}" max="${widget.properties.max}" step="${widget.properties.step}" value="${widget.properties.value}">
                            </div>
                        </div>`;
                    settingsHTML += html;
                    break;
            }
        });
        
        settingsContainer.innerHTML = settingsHTML;
        telemetryContainer.innerHTML = telemetryHTML;
        
        // Dinamik olarak oluşturulan elemanlar için olay dinleyicilerini yeniden bağla
        setupDynamicEvents(schema.widgets);

    } catch (error) {
        console.error("Dynamic UI Render Failed:", error);
    }
}

function setupDynamicEvents(widgets) {
     widgets.forEach(widget => {
        if (widget.type === 'slider') {
            const input = $(widget.id);
            const display = $(widget.display_id);
            if(input && display) {
                input.oninput = (e) => display.innerText = e.target.value;
            }
        }
     });
}

// --- CHARTS ---
function setupCharts() {
    const ctx = $('tpsChart');
    if(!ctx) return;
    
    Store.chart = new Chart(ctx, {
        type: 'line',
        data: {
            labels: Store.chartData.labels,
            datasets: [{
                data: Store.chartData.data,
                borderColor: '#8b5cf6',
                borderWidth: 2,
                tension: 0.4,
                pointRadius: 0,
                fill: true,
                backgroundColor: 'rgba(139, 92, 246, 0.1)'
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            plugins: { legend: { display: false } },
            scales: { x: { display: false }, y: { display: false, min: 0 } },
            animation: false
        }
    });
}

function updateStats() {
    const dur = (Date.now() - Store.startTime) / 1000;
    const tps = Store.tokenCount / (dur || 1);
    
    if($('tpsBig')) $('tpsBig').innerText = tps.toFixed(1);
    if($('latencyStat')) $('latencyStat').innerText = Math.round(dur * 1000) + 'ms';
    
    if(Store.chart) {
        Store.chartData.data.push(tps);
        Store.chartData.data.shift();
        Store.chart.update();
    }
}

// --- MODEL ---
function renderModelList() {
    const matrix = $('modelMatrix');
    if (!matrix) return;
    matrix.innerHTML = '';
    MODEL_CATALOG.forEach(model => {
        const div = document.createElement('div');
        div.className = `matrix-item ${model.id === Store.currentProfile ? 'active' : ''}`;
        div.innerHTML = `
            <div><span class="m-name"><i class="${model.icon}"></i> ${model.name}</span></div>
            <span class="m-desc">${model.desc}</span>
        `;
        div.onclick = () => switchModel(model.id, model.name);
        matrix.appendChild(div);
    });
}

function toggleModelMatrix() {
    const el = $('modelMatrix');
    if(el) el.classList.toggle('hidden');
}

document.addEventListener('click', (e) => {
    const dock = document.querySelector('.model-dock');
    const matrix = $('modelMatrix');
    if (dock && matrix && !dock.contains(e.target)) {
        matrix.classList.add('hidden');
    }
});

async function switchModel(id, name) {
    $('modelMatrix').classList.add('hidden');
    if (id === Store.currentProfile) return;
    if (!confirm(`${name} modeline geçilsin mi?`)) return;

    Store.isSwitching = true;
    $('systemOverlay').classList.remove('hidden');

    try {
        const res = await fetch('/v1/models/switch', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({ profile: id })
        });
        const data = await res.json();
        
        if (res.ok && data.status === 'success') {
            Store.currentProfile = id;
            $('activeModelName').innerText = name;
            renderModelList();
            addMessage('system', `SYSTEM: Model switched to **${name}**`);
        } else {
            throw new Error(data.message);
        }
    } catch (e) {
        alert("Switch Failed: " + e.message);
    } finally {
        Store.isSwitching = false;
        $('systemOverlay').classList.add('hidden');
    }
}

// --- CHAT ---
async function sendMessage() {
    const input = $('omniInput');
    const text = input.value.trim();
    if (!text) return;

    input.value = '';
    input.style.height = 'auto';
    setBusy(true);
    
    addMessage('user', escapeHtml(text));
    Store.history.push({role: 'user', content: text});
    
    const bubble = addMessage('ai', '...');
    Store.startTime = Date.now();
    Store.tokenCount = 0;
    Store.controller = new AbortController();
    
    let fullText = "";
    
    try {
        const payload = buildPayload(text);
        if($('debugLog')) $('debugLog').innerText = JSON.stringify(payload, null, 2);

        const res = await fetch('/v1/chat/completions', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify(payload),
            signal: Store.controller.signal
        });

        if (!res.ok) throw new Error(res.status === 503 ? "Model meşgul/yükleniyor..." : "API Hatası");

        const reader = res.body.getReader();
        const decoder = new TextDecoder();

        while (true) {
            const {done, value} = await reader.read();
            if (done) break;
            const chunk = decoder.decode(value, {stream: true});
            const lines = chunk.split('\n');
            for (const line of lines) {
                if (line.startsWith('data: ') && line !== 'data: [DONE]') {
                    try {
                        const json = JSON.parse(line.substring(6));
                        const content = json.choices[0]?.delta?.content;
                        if (content) {
                            fullText += content;
                            Store.tokenCount++;
                            if(Store.tokenCount % 3 === 0) {
                                bubble.innerHTML = marked.parse(fullText);
                                updateStats();
                                scrollToBottom();
                            }
                        }
                    } catch(e) {}
                }
            }
        }
        bubble.innerHTML = marked.parse(fullText);
        enhanceCode(bubble);
        Store.history.push({role: 'assistant', content: fullText});

    } catch (e) {
        if (e.name !== 'AbortError') {
            bubble.innerHTML = `<span style="color:#ef4444">Error: ${e.message}</span>`;
        }
    } finally {
        setBusy(false);
        scrollToBottom();
    }
}

function buildPayload(text) {
    const sys = $('systemPrompt') ? $('systemPrompt').value : "";
    const rag = $('ragInput') ? $('ragInput').value : ""; // Dinamik olarak oluşturulmuş elemandan oku
    
    const msgs = [];
    if (rag) msgs.push({ role: 'system', content: `CONTEXT:\n${rag}` });
    if (sys) msgs.push({ role: 'system', content: sys });
    
    // Geçmiş limiti artık HTML'de değil, sabit varsayalım
    Store.history.slice(-10).forEach(m => msgs.push(m));
    
    msgs.push({ role: 'user', content: text });

    const tempEl = $('tempInput'); // Dinamik
    const tokEl = $('tokenLimit'); // Statik

    return {
        messages: msgs,
        temperature: tempEl ? parseFloat(tempEl.value) : 0.7,
        max_tokens: tokEl ? parseInt(tokEl.value) : 1024,
        stream: true
    };
}

// --- UTILS ---
function setupEvents() {
    const inp = $('omniInput');
    if(inp) {
        inp.addEventListener('input', (e) => {
            e.target.style.height = 'auto';
            e.target.style.height = Math.min(e.target.scrollHeight, 150) + 'px';
        });
        inp.addEventListener('keydown', (e) => {
            if (e.key === 'Enter' && !e.shiftKey) {
                e.preventDefault();
                if(!Store.generating) sendMessage();
            }
        });
    }

    const send = $('sendBtn');
    if(send) send.onclick = sendMessage;
    
    const stop = $('stopBtn');
    if(stop) stop.onclick = () => { if(Store.controller) Store.controller.abort(); };

    // Sadece statik kalan slider için
    const tokEl = $('tokenLimit');
    const tokVal = $('tokenVal');
    if(tokEl && tokVal) {
        tokEl.oninput = (e) => tokVal.innerText = e.target.value;
    }
}

function setPersona(key) {
    document.querySelectorAll('.chip').forEach(b => b.classList.remove('active'));
    const btns = document.querySelectorAll('.chip');
    btns.forEach(b => {
        if(b.innerText.toLowerCase().includes(key) || b.onclick.toString().includes(key)) 
            b.classList.add('active');
    });
    
    const sysPrompt = $('systemPrompt');
    if(sysPrompt && PERSONAS[key]) sysPrompt.value = PERSONAS[key];
}

function addMessage(role, content) {
    const container = $('streamContainer');
    const empty = container.querySelector('.empty-void');
    if(empty) empty.remove();

    const div = document.createElement('div');
    div.className = `msg-block ${role}`;
    div.innerHTML = `
        <div class="msg-avatar"><i class="fas fa-${role==='user'?'user':'robot'}"></i></div>
        <div class="msg-content">${role==='user'?content:''}</div>
    `;
    container.appendChild(div);
    return div.querySelector('.msg-content');
}

function scrollToBottom() {
    const el = $('streamContainer');
    if(el) el.scrollTop = el.scrollHeight;
}

function setBusy(busy) {
    Store.generating = busy;
    const send = $('sendBtn');
    const stop = $('stopBtn');
    if(send) send.style.display = busy ? 'none' : 'flex';
    if(stop) stop.classList.toggle('hidden', !busy);
}

function escapeHtml(text) {
    return text.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
}

async function checkHealth() {
    const el = $('activeModelName');
    const trigger = $('modelTrigger');
    try {
        const res = await fetch('/health');
        if(res.ok && el.innerText === "Bağlanıyor...") {
            const active = MODEL_CATALOG.find(m => m.id === Store.currentProfile);
            el.innerText = active ? active.name : "SYSTEM READY";
            if(trigger) trigger.querySelector('.status-dot').className = "status-dot online";
        }
    } catch {
        if(trigger) trigger.querySelector('.status-dot').className = "status-dot";
    }
}

function enhanceCode(el) {
    el.querySelectorAll('pre code').forEach(block => hljs.highlightElement(block));
}

window.togglePanel = (id) => {
    const p = $(id);
    document.querySelectorAll('.slide-panel').forEach(pan => {
        if(pan.id !== id) pan.classList.remove('open');
    });
    if(p) p.classList.toggle('open');
}

window.downloadHistory = () => { /* ... */ }
window.clearChat = () => $('streamContainer').innerHTML = `<div class="empty-void"><div class="void-icon"><i class="fas fa-bolt"></i></div><h1>Agent Ready</h1><p>Görevi başlatın.</p></div>`;
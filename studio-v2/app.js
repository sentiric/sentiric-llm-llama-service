/**
 * SENTIRIC AGENT OS v7.0 (LLM-DRIVEN UI)
 *
 * Bu dosya, arayüzün tüm mantığını içerir.
 * UI elemanları, sayfa yüklendiğinde /v1/ui/layout endpoint'inden alınan
 * bir JSON şemasına göre dinamik olarak oluşturulur.
 */

const $ = (id) => document.getElementById(id);

// --- GLOBAL STATE ---
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

// --- UYGULAMA BAŞLANGICI ---
document.addEventListener('DOMContentLoaded', () => {
    console.log("🌌 Sentiric OS v7.0 Booting (LLM-DRIVEN UI)...");
    
    renderModelList();
    renderDynamicLayout(); // Çekirdek dinamik render fonksiyonu
    setupStaticEvents();
    setupCharts();
    checkHealth();
    setInterval(checkHealth, 5000);
});


// --- BÖLÜM 1: DİNAMİK UI OLUŞTURMA ---

async function renderDynamicLayout() {
    try {
        const res = await fetch('/v1/ui/layout');
        if (!res.ok) throw new Error('Layout schema could not be fetched.');
        const schema = await res.json();

        const panelContainers = {
            settings: $('dynamic-controls'),
            telemetry: $('dynamic-telemetry-controls')
        };
        
        Object.keys(schema.panels).forEach(panelKey => {
            const container = panelContainers[panelKey];
            const widgets = schema.panels[panelKey];
            if (container && widgets) {
                container.innerHTML = widgets.map(renderWidget).join('');
            }
        });
        
        setupDynamicEvents(schema);
        setPersona('default'); // Default persona'yı uygula

    } catch (error) {
        console.error("Dynamic UI Render Failed:", error);
        $('dynamic-controls').innerHTML = `<div class="error-msg">UI yüklenemedi.</div>`;
    }
}

function renderWidget(widget) {
    switch(widget.type) {
        case 'chip-group':
            return `
                <div class="setting-group">
                    <label>${widget.label}</label>
                    <div class="chip-grid" id="${widget.id}">
                        ${widget.options.map(opt => `
                            <button class="chip ${opt.active ? 'active' : ''}" onclick="setPersona('${opt.value}')">${opt.label}</button>
                        `).join('')}
                    </div>
                </div>`;
        case 'textarea':
            return `
                <div class="setting-group">
                    <label>${widget.label}</label>
                    <textarea id="${widget.id}" class="code-area" placeholder="${widget.properties.placeholder || ''}" rows="${widget.properties.rows || 3}"></textarea>
                </div>`;
        case 'slider':
            return `
                <div class="setting-group">
                    <div class="range-wrap">
                        <div class="range-head"><span>${widget.label}</span><span id="${widget.display_id}">${widget.properties.value}</span></div>
                        <input type="range" id="${widget.id}" min="${widget.properties.min}" max="${widget.properties.max}" step="${widget.properties.step}" value="${widget.properties.value}">
                    </div>
                </div>`;
        default: return '';
    }
}

function setupDynamicEvents(schema) {
    Object.values(schema.panels).flat().forEach(widget => {
        if (widget.type === 'slider') {
            const input = $(widget.id);
            const display = $(widget.display_id);
            if(input && display) {
                input.oninput = (e) => display.innerText = e.target.value;
            }
        }
    });
}

// --- BÖLÜM 2: SOHBET VE İSTEK YÖNETİMİ ---

async function sendMessage() {
    const input = $('omniInput');
    const text = input.value.trim();
    if (!text || Store.generating) return;

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

        if (!res.ok) throw new Error(res.status === 503 ? "Model meşgul veya yükleniyor..." : `API Hatası: ${res.statusText}`);

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
                            if(Store.tokenCount % 3 === 0) { // Her 3 token'da bir DOM güncelle
                                bubble.innerHTML = marked.parse(fullText);
                                updateStats();
                                scrollToBottom();
                            }
                        }
                    } catch(e) { /* Hatalı JSON parçalarını yoksay */ }
                }
            }
        }
        bubble.innerHTML = marked.parse(fullText);
        enhanceCode(bubble);
        Store.history.push({role: 'assistant', content: fullText});

    } catch (e) {
        if (e.name !== 'AbortError') {
            bubble.innerHTML = `<span style="color:#ef4444">Hata: ${e.message}</span>`;
        }
    } finally {
        setBusy(false);
        scrollToBottom();
    }
}

function buildPayload(text) {
    const sys = $('systemPrompt')?.value || "";
    const rag = $('ragInput')?.value || "";
    
    const msgs = [];
    if (rag) msgs.push({ role: 'system', content: `CONTEXT:\n${rag}` });
    if (sys) msgs.push({ role: 'system', content: sys });
    
    Store.history.slice(-10).forEach(m => msgs.push(m)); // Son 10 mesajı geçmiş olarak al
    msgs.push({ role: 'user', content: text });

    return {
        messages: msgs,
        temperature: parseFloat($('tempInput')?.value || 0.7),
        max_tokens: parseInt($('tokenLimit')?.value || 1024),
        stream: true
    };
}

// --- BÖLÜM 3: MODEL YÖNETİMİ ---

function renderModelList() {
    const matrix = $('modelMatrix');
    if (!matrix) return;
    matrix.innerHTML = '';
    MODEL_CATALOG.forEach(model => {
        const div = document.createElement('div');
        div.className = `matrix-item ${model.id === Store.currentProfile ? 'active' : ''}`;
        div.innerHTML = `<div><span class="m-name"><i class="${model.icon}"></i> ${model.name}</span></div><span class="m-desc">${model.desc}</span>`;
        div.onclick = () => switchModel(model.id, model.name);
        matrix.appendChild(div);
    });
}

async function switchModel(id, name) {
    $('modelMatrix').classList.add('hidden');
    if (id === Store.currentProfile || Store.isSwitching) return;
    if (!confirm(`${name} modeline geçilsin mi? Bu işlem biraz zaman alabilir.`)) return;

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
            addMessage('system', `SİSTEM: Model başarıyla **${name}** olarak değiştirildi.`);
        } else {
            throw new Error(data.message || 'Bilinmeyen bir hata oluştu.');
        }
    } catch (e) {
        alert("Model Değiştirme Başarısız: " + e.message);
        addMessage('system', `<span style="color:#ef4444">HATA: Model değiştirilemedi.</span>`);
    } finally {
        Store.isSwitching = false;
        // Sağlık kontrolü başarılı olana kadar overlay'i göstermeye devam et
        await new Promise(resolve => setTimeout(resolve, 2000)); // Kısa bir gecikme
        let isHealthy = false;
        while (!isHealthy) {
            isHealthy = await checkHealth(true);
            if (!isHealthy) await new Promise(resolve => setTimeout(resolve, 3000));
        }
        $('systemOverlay').classList.add('hidden');
    }
}

// --- BÖLÜM 4: YARDIMCI FONKSİYONLAR VE OLAY DİNLEYİCİLER ---

function setupStaticEvents() {
    const inp = $('omniInput');
    if(inp) {
        inp.addEventListener('input', (e) => {
            e.target.style.height = 'auto';
            e.target.style.height = Math.min(e.target.scrollHeight, 150) + 'px';
        });
        inp.addEventListener('keydown', (e) => {
            if (e.key === 'Enter' && !e.shiftKey) {
                e.preventDefault();
                sendMessage();
            }
        });
    }
    const send = $('sendBtn');
    if(send) send.onclick = sendMessage;
    
    const stop = $('stopBtn');
    if(stop) stop.onclick = () => { if(Store.controller) Store.controller.abort(); };

    document.addEventListener('click', (e) => {
        const dock = document.querySelector('.model-dock');
        const matrix = $('modelMatrix');
        if (dock && matrix && !dock.contains(e.target)) {
            matrix.classList.add('hidden');
        }
    });
}

function setPersona(key) {
    const container = $('persona-chips');
    if (!container) return;
    container.querySelectorAll('.chip').forEach(b => b.classList.remove('active'));
    container.querySelectorAll('.chip').forEach(b => {
        if(b.onclick.toString().includes(`'${key}'`)) b.classList.add('active');
    });

    const personaPrompts = {
        'default': "Sen yardımsever bir asistansın. Kısa, net ve Türkçe cevaplar ver.",
        'coder': "Sen uzman bir yazılım mühendisisin. Temiz, etkili ve iyi belgelenmiş kodlar sun.",
        'creative': "Sen yaratıcı bir yazarsın. Hikayeler, şiirler ve senaryolar üretebilirsin.",
        'english': "You are a helpful assistant. Respond in English."
    };
    const sysPrompt = $('systemPrompt');
    if(sysPrompt && personaPrompts[key]) {
        sysPrompt.value = personaPrompts[key];
    }
}

function addMessage(role, content) {
    const container = $('streamContainer');
    const empty = container.querySelector('.empty-void');
    if(empty) empty.remove();

    const div = document.createElement('div');
    div.className = `msg-block ${role}`;
    div.innerHTML = `
        <div class="msg-avatar"><i class="fas fa-${role==='user'?'user':(role === 'system' ? 'info-circle' : 'robot')}"></i></div>
        <div class="msg-content">${role !== 'ai' ? marked.parse(content) : '...'}</div>
    `;
    if (role === 'system') div.classList.add('system');
    container.appendChild(div);
    return div.querySelector('.msg-content');
}

function scrollToBottom() { const el = $('streamContainer'); if(el) el.scrollTop = el.scrollHeight; }
function setBusy(busy) { Store.generating = busy; const send = $('sendBtn'); const stop = $('stopBtn'); if(send) send.style.display = busy ? 'none' : 'flex'; if(stop) stop.classList.toggle('hidden', !busy); }
function escapeHtml(text) { return text.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;"); }
function enhanceCode(el) { el.querySelectorAll('pre code').forEach(block => hljs.highlightElement(block)); }
async function checkHealth(returnStatus = false) {
    const el = $('activeModelName');
    const trigger = $('modelTrigger');
    try {
        const res = await fetch('/health');
        const data = await res.json();
        if (res.ok && data.status === 'healthy') {
            if (el && el.innerText === "Bağlanıyor...") {
                 const active = MODEL_CATALOG.find(m => m.id === Store.currentProfile);
                 el.innerText = active ? active.name : "SİSTEM HAZIR";
            }
            if(trigger) trigger.querySelector('.status-dot').className = "status-dot online";
            if (returnStatus) return true;
        } else {
            if(trigger) trigger.querySelector('.status-dot').className = "status-dot loading";
             if (returnStatus) return false;
        }
    } catch {
        if(trigger) trigger.querySelector('.status-dot').className = "status-dot";
        if (returnStatus) return false;
    }
}

// --- İSTATİSTİK VE GRAFİKLER ---
function setupCharts() {
    const ctx = $('tpsChart');
    if(!ctx) return;
    Store.chart = new Chart(ctx, {
        type: 'line',
        data: { labels: Store.chartData.labels, datasets: [{ data: Store.chartData.data, borderColor: '#8b5cf6', borderWidth: 2, tension: 0.4, pointRadius: 0, fill: true, backgroundColor: 'rgba(139, 92, 246, 0.1)' }] },
        options: { responsive: true, maintainAspectRatio: false, plugins: { legend: { display: false } }, scales: { x: { display: false }, y: { display: false, min: 0 } }, animation: false }
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

// --- GLOBAL PENCERE FONKSİYONLARI ---
window.togglePanel = (id) => { const p = $(id); document.querySelectorAll('.slide-panel').forEach(pan => { if(pan.id !== id) pan.classList.remove('open'); }); if(p) p.classList.toggle('open'); };
window.clearChat = () => { $('streamContainer').innerHTML = `<div class="empty-void"><div class="void-icon"><i class="fas fa-bolt"></i></div><h1>Agent Ready</h1><p>Bir görev verin veya dosya yükleyin.</p></div>`; Store.history = []; };
/**
 * SENTIRIC AGENT OS v7.4 (LLM-DRIVEN UI - STABLE FIX)
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

// --- UYGULAMA BAŞLANGICI ---
document.addEventListener('DOMContentLoaded', async () => {
    console.log("🌌 Sentiric OS v7.4 Booting (Stable Fix)...");
    
    await renderModelListFromAPI();
    await renderDynamicLayout();
    
    setupStaticEvents();
    setupCharts();
    
    await checkHealth();
    setInterval(checkHealth, 5000);
});

// --- BÖLÜM 1: DİNAMİK UI ve MODEL YÖNETİMİ ---

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
        setPersona('default');

    } catch (error) {
        console.error("Dynamic UI Render Failed:", error);
        $('dynamic-controls').innerHTML = `<div class="error-msg">UI yüklenemedi.</div>`;
    }
}

// --- KRİTİK DÜZELTME: EKSİK FONKSİYON BURAYA EKLENDİ ---
function renderWidget(widget) {
    switch(widget.type) {
        case 'chip-group':
            return `
                <div class="setting-group">
                    <label>${widget.label}</label>
                    <div class="chip-grid" id="${widget.id}">
                        ${widget.options.map(opt => `
                            <button class="chip ${opt.active ? 'active' : ''}" data-persona-key="${opt.value}">${opt.label}</button>
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

async function renderModelListFromAPI() {
    try {
        const res = await fetch('/v1/profiles');
        if (!res.ok) throw new Error('Profiles could not be fetched.');
        const data = await res.json();
        
        Store.currentProfile = data.active_profile;

        const matrix = $('modelMatrix');
        if (!matrix) return;
        matrix.innerHTML = '';

        for (const key in data.profiles) {
            const profile = data.profiles[key];
            const div = document.createElement('div');
            div.className = `matrix-item ${key === Store.currentProfile ? 'active' : ''}`;
            div.innerHTML = `<div><span class="m-name">${profile.description.split('[')[0].trim()}</span></div><span class="m-desc">${profile.description.split(']')[1]?.trim() || ''}</span>`;
            div.onclick = () => switchModel(key, profile.description.split('[')[0].trim());
            matrix.appendChild(div);
        }
        
        const activeProfileName = data.profiles[Store.currentProfile]?.description.split('[')[0].trim() || 'SİSTEM';
        const activeModelEl = $('activeModelName');
        if (activeModelEl) activeModelEl.innerText = activeProfileName;

    } catch (error) {
        console.error("Model list render failed:", error);
        const matrix = $('modelMatrix');
        if (matrix) matrix.innerHTML = `<div class="error-msg">Profiller yüklenemedi.</div>`;
    }
}

async function switchModel(profileKey, profileName) {
    $('modelMatrix').classList.add('hidden');
    if (profileKey === Store.currentProfile || Store.isSwitching) return;
    if (!confirm(`${profileName} profiline geçilsin mi? Bu işlem biraz zaman alabilir.`)) return;

    Store.isSwitching = true;
    $('systemOverlay').classList.remove('hidden');
    $('activeModelName').innerText = "Yükleniyor...";

    try {
        const res = await fetch('/v1/models/switch', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({ profile: profileKey })
        });
        const data = await res.json();
        
        if (res.ok && data.status === 'success') {
            Store.currentProfile = profileKey;
            addMessage('system', `SİSTEM: Model başarıyla **${profileName}** olarak değiştirildi.`);
        } else {
            throw new Error(data.message || 'Bilinmeyen bir hata oluştu.');
        }
    } catch (e) {
        alert("Model Değiştirme Başarısız: " + e.message);
        addMessage('system', `<span style="color:#ef4444">HATA: Model değiştirilemedi.</span>`);
    } finally {
        Store.isSwitching = false;
        await renderModelListFromAPI();
        
        let isHealthy = false;
        let retries = 10;
        while (!isHealthy && retries > 0) {
            isHealthy = await checkHealth(true);
            if (!isHealthy) await new Promise(resolve => setTimeout(resolve, 3000));
            retries--;
        }
        $('systemOverlay').classList.add('hidden');
    }
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
    
    const bubble = addMessage('ai', '');
    Store.startTime = Date.now();
    Store.tokenCount = 0;
    Store.controller = new AbortController();
    
    let fullText = "";
    let thinkingIndicatorRemoved = false;
    
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
                            if (!thinkingIndicatorRemoved) {
                                bubble.innerHTML = '';
                                thinkingIndicatorRemoved = true;
                            }
                            fullText += content;
                            Store.tokenCount++;
                            bubble.innerHTML = marked.parse(fullText + '▋');
                            updateStats();
                            scrollToBottom();
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
            bubble.innerHTML = `<span style="color:#ef4444">Hata: ${e.message}</span>`;
        } else {
             bubble.innerHTML = `<span style="color:#facc15">İptal edildi.</span>`;
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
    
    if (Store.history.length <= 1) { 
        if (rag) msgs.push({ role: 'system', content: `CONTEXT:\n${rag}` });
        if (sys) msgs.push({ role: 'system', content: sys });
    }
    
    const historySlice = Store.history.slice(-10);
    historySlice.forEach(m => {
        if (m.role === 'user' && m.content === text && historySlice.indexOf(m) === historySlice.length-1) return;
        msgs.push(m);
    });

    msgs.push({ role: 'user', content: text });

    return {
        messages: msgs,
        temperature: parseFloat($('tempInput')?.value || 0.7),
        max_tokens: parseInt($('tokenLimit')?.value || 1024),
        stream: true
    };
}


// --- BÖLÜM 3: YARDIMCI FONKSİYONLAR ---

function addMessage(role, content) {
    const container = $('streamContainer');
    const empty = container.querySelector('.empty-void');
    if(empty) empty.remove();

    const div = document.createElement('div');
    div.className = `msg-block ${role}`;
    
    let contentHTML = '';
    if (role === 'ai' && content === '') {
        contentHTML = `<div class="typing-indicator"><span></span><span></span><span></span></div>`;
    } else {
        contentHTML = marked.parse(content);
    }

    div.innerHTML = `
        <div class="msg-avatar"><i class="fas fa-${role==='user'?'user':(role === 'system' ? 'info-circle' : 'robot')}"></i></div>
        <div class="msg-content">${contentHTML}</div>
    `;
    if (role === 'system') div.classList.add('system');
    
    container.appendChild(div);
    scrollToBottom();
    return div.querySelector('.msg-content');
}

function setupStaticEvents() {
    const inp = $('omniInput'); if(inp) { inp.addEventListener('input', (e) => { e.target.style.height = 'auto'; e.target.style.height = Math.min(e.target.scrollHeight, 150) + 'px'; }); inp.addEventListener('keydown', (e) => { if (e.key === 'Enter' && !e.shiftKey) { e.preventDefault(); sendMessage(); } }); }
    const send = $('sendBtn'); if(send) send.onclick = sendMessage; const stop = $('stopBtn'); if(stop) stop.onclick = () => { if(Store.controller) Store.controller.abort(); };
}

function setupDynamicEvents(schema) {
    Object.values(schema.panels).flat().forEach(widget => {
        if (widget.type === 'slider') { const input = $(widget.id); const display = $(widget.display_id); if(input && display) { input.oninput = (e) => display.innerText = e.target.value; } }
        if (widget.type === 'chip-group') { const container = $(widget.id); if (container) { container.querySelectorAll('.chip').forEach(chip => { chip.onclick = () => setPersona(chip.dataset.personaKey); }); } }
    });
}

function setPersona(key) {
    const container = $('persona-chips'); if (!container) return; container.querySelectorAll('.chip').forEach(b => b.classList.remove('active')); container.querySelector(`[data-persona-key="${key}"]`)?.classList.add('active');
    const personaPrompts = { 'default': "Sen yardımsever bir asistansın. Kısa, net ve Türkçe cevaplar ver.", 'coder': "Sen uzman bir yazılım mühendisisin. Temiz, etkili ve iyi belgelenmiş kodlar sun.", 'creative': "Sen yaratıcı bir yazarsın. Hikayeler, şiirler ve senaryolar üretebilirsin.", 'english': "You are a helpful assistant. Respond in English." };
    const sysPrompt = $('systemPrompt'); if(sysPrompt && personaPrompts[key]) { sysPrompt.value = personaPrompts[key]; }
}

function scrollToBottom() { const el = $('streamContainer'); if(el) el.scrollTop = el.scrollHeight; }
function setBusy(busy) { Store.generating = busy; const send = $('sendBtn'); const stop = $('stopBtn'); if(send) send.style.display = busy ? 'none' : 'flex'; if(stop) stop.classList.toggle('hidden', !busy); }
function escapeHtml(text) { return text.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;"); }
function enhanceCode(el) { el.querySelectorAll('pre code').forEach(block => hljs.highlightElement(block)); }

async function checkHealth(returnStatus = false) {
    const el = $('activeModelName'); const trigger = $('modelTrigger');
    try {
        const res = await fetch('/health'); const data = await res.json();
        if (res.ok && data.status === 'healthy') {
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

// --- BÖLÜM 4: İSTATİSTİK ve GRAFİKLER ---

function setupCharts() {
    const ctx = $('tpsChart'); if(!ctx) return;
    Store.chart = new Chart(ctx, { type: 'line', data: { labels: Store.chartData.labels, datasets: [{ data: Store.chartData.data, borderColor: '#8b5cf6', borderWidth: 2, tension: 0.4, pointRadius: 0, fill: true, backgroundColor: 'rgba(139, 92, 246, 0.1)' }] }, options: { responsive: true, maintainAspectRatio: false, plugins: { legend: { display: false } }, scales: { x: { display: false }, y: { display: false, min: 0 } }, animation: false } });
}
function updateStats() {
    const dur = (Date.now() - Store.startTime) / 1000; const tps = Store.tokenCount / (dur || 1);
    if($('tpsBig')) $('tpsBig').innerText = tps.toFixed(1); if($('latencyStat')) $('latencyStat').innerText = Math.round(dur * 1000) + 'ms';
    if(Store.chart) { Store.chartData.data.push(tps); Store.chartData.data.shift(); Store.chart.update(); }
}

// --- BÖLÜM 5: GLOBAL FONKSİYONLAR ---

window.toggleModelMatrix = () => { const el = $('modelMatrix'); if(el) el.classList.toggle('hidden'); };
window.togglePanel = (id) => { const p = $(id); document.querySelectorAll('.slide-panel').forEach(pan => { if(pan.id !== id) pan.classList.remove('open'); }); if(p) p.classList.toggle('open'); };
window.clearChat = () => { $('streamContainer').innerHTML = `<div class="empty-void"><div class="void-icon"><i class="fas fa-bolt"></i></div><h1>Agent Ready</h1><p>Bir görev verin veya dosya yükleyin.</p></div>`; Store.history = []; };
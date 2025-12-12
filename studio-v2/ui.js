import { $, Store, PERSONAS } from './state.js';

export function renderWidget(widget) {
    switch(widget.type) {
        case 'chip-group':
            return `
                <div class="setting-group">
                    <label>${widget.label}</label>
                    <div class="chip-grid" id="${widget.id}">
                        ${widget.options.map(opt => `<button class="chip ${opt.active ? 'active' : ''}" data-persona-key="${opt.value}">${opt.label}</button>`).join('')}
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

export function renderModelList(profilesData) {
    Store.currentProfile = profilesData.active_profile;
    const selector = $('modelSelector');
    if (!selector) return;
    selector.innerHTML = '';

    const groupedProfiles = {};
    for (const key in profilesData.profiles) {
        const profile = profilesData.profiles[key];
        if (!groupedProfiles[profile.category]) groupedProfiles[profile.category] = [];
        groupedProfiles[profile.category].push({ key, ...profile });
    }

    for (const category in groupedProfiles) {
        const group = document.createElement('optgroup');
        group.label = category;
        groupedProfiles[category].forEach(profile => {
            const option = document.createElement('option');
            option.value = profile.key;
            option.textContent = `${profile.display_name} - (${profile.description})`;
            if (profile.key === Store.currentProfile) option.selected = true;
            group.appendChild(option);
        });
        selector.appendChild(group);
    }
}

// --- MESAJ OLUŞTURMA (GÜNCELLENDİ) ---
// Yapıyı ayırıyoruz: Mesaj Konteynırı > Düşünce Kutusu (Opsiyonel) + Cevap Metni
export function addMessage(role, content) {
    const container = $('streamContainer');
    const empty = container.querySelector('.empty-void');
    if (empty) empty.remove();

    const div = document.createElement('div');
    div.className = `msg-block ${role}`;
    
    // Mesaj yapısı: Avatar + İçerik Kutusu
    let innerHTML = `<div class="msg-avatar"><i class="fas fa-${role==='user'?'user':(role === 'system' ? 'info-circle' : 'robot')}"></i></div>`;
    innerHTML += `<div class="msg-content">`;
    
    // AI ise ve başlangıçta boşsa (yeni mesaj), düşünce kutusu yer tutucusu ve metin alanı oluştur
    if (role === 'ai') {
        innerHTML += `<div class="thought-box" style="display:none;"><div class="thought-header"><i class="fas fa-caret-right"></i> Düşünce Süreci</div><div class="thought-body"></div></div>`;
        innerHTML += `<div class="text-body">${content === '' ? '<div class="typing-indicator"><span></span><span></span><span></span></div>' : marked.parse(content)}</div>`;
    } else {
        innerHTML += `<div class="text-body">${marked.parse(content)}</div>`;
    }
    innerHTML += `</div>`; // Close msg-content

    div.innerHTML = innerHTML;
    if (role === 'system') div.classList.add('system');
    
    container.appendChild(div);
    scrollToBottom();

    // Düşünce kutusuna tıklama olayı
    if(role === 'ai') {
        const tHeader = div.querySelector('.thought-header');
        if(tHeader) {
            tHeader.onclick = () => {
                div.querySelector('.thought-box').classList.toggle('open');
            };
        }
        return {
            content: div.querySelector('.text-body'),
            thoughtBox: div.querySelector('.thought-box'),
            thoughtBody: div.querySelector('.thought-body')
        };
    }
    
    return div.querySelector('.text-body');
}

export function setPersona(key) {
    const container = $('persona-chips');
    if (!container) return;
    container.querySelectorAll('.chip').forEach(b => b.classList.remove('active'));
    const activeChip = container.querySelector(`[data-persona-key="${key}"]`);
    if(activeChip) activeChip.classList.add('active');

    const sysPrompt = $('systemPrompt');
    if (sysPrompt && PERSONAS[key]) {
        sysPrompt.value = PERSONAS[key];
    }
}

export function updateHealthStatus(isHealthy) {
    const statusIndicator = $('modelStatusIndicator');
    if (statusIndicator) {
        statusIndicator.className = `status-dot ${isHealthy ? 'online' : (Store.isSwitching ? 'loading' : '')}`;
    }
}

export function togglePanel(panelId) {
    const p = $(panelId);
    if (!p) return;
    const isOpen = p.classList.contains('open');
    document.querySelectorAll('.slide-panel').forEach(pan => pan.classList.remove('open'));
    if (!isOpen) p.classList.add('open');
}

export function setBusy(busy) {
    Store.generating = busy;
    $('sendBtn').style.display = busy ? 'none' : 'flex';
    $('stopBtn').classList.toggle('hidden', !busy);
}

export function updateStats() {
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

export function setupCharts() {
    const ctx = $('tpsChart');
    if(!ctx) return;
    Store.chart = new Chart(ctx, { type: 'line', data: { labels: Store.chartData.labels, datasets: [{ data: Store.chartData.data, borderColor: '#8b5cf6', borderWidth: 2, tension: 0.4, pointRadius: 0, fill: true, backgroundColor: 'rgba(139, 92, 246, 0.1)' }] }, options: { responsive: true, maintainAspectRatio: false, plugins: { legend: { display: false } }, scales: { x: { display: false }, y: { display: false, min: 0 } }, animation: false } });
}

export const escapeHtml = (text) => text.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
export const enhanceCode = (el) => el.querySelectorAll('pre code').forEach(block => hljs.highlightElement(block));
export const scrollToBottom = () => { const el = $('streamContainer'); if(el) el.scrollTop = el.scrollHeight; };
export const clearChat = () => { $('streamContainer').innerHTML = `<div class="empty-void"><div class="void-icon"><i class="fas fa-bolt"></i></div><h1>Agent Ready</h1><p>Bir görev verin veya dosya yükleyin.</p></div>`; Store.history = []; };
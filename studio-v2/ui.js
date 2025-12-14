import { $, Store, PERSONAS } from './state.js';

// --- GÜVENLİK ---
export const escapeHtml = (text) => {
    if (!text) return text;
    return text
        .replace(/&/g, "&amp;")
        .replace(/</g, "&lt;")
        .replace(/>/g, "&gt;")
        .replace(/"/g, "&quot;")
        .replace(/'/g, "&#039;");
};

// --- WIDGET RENDERER ---
export function renderWidget(widget) {
    try {
        switch(widget.type) {
            case 'segmented':
                return `
                    <div class="setting-group">
                        <label>${escapeHtml(widget.label)}</label>
                        <div class="segmented-control" id="${widget.id}">
                            ${widget.options.map(opt => `
                                <label>
                                    <input type="radio" name="${widget.id}" value="${opt.value}" ${opt.active ? 'checked' : ''}>
                                    <span>${escapeHtml(opt.label)}</span>
                                </label>
                            `).join('')}
                        </div>
                    </div>`;
            case 'chip-group':
                return `
                    <div class="setting-group">
                        <label>${escapeHtml(widget.label)}</label>
                        <div class="chip-grid" id="${widget.id}">
                            ${widget.options.map(opt => `<button class="chip ${opt.active ? 'active' : ''}" data-persona-key="${opt.value}">${escapeHtml(opt.label)}</button>`).join('')}
                        </div>
                    </div>`;
            case 'hardware-slider':
                return `
                    <div class="setting-group hardware-control">
                        <div class="range-wrap">
                            <div class="range-head">
                                <span style="color:#facc15"><i class="fas fa-microchip"></i> ${escapeHtml(widget.label)}</span>
                                <span id="${widget.display_id}">${widget.properties.value}</span>
                            </div>
                            <input type="range" id="${widget.id}" min="${widget.properties.min}" max="${widget.properties.max}" step="${widget.properties.step}" value="${widget.properties.value}">
                        </div>
                    </div>`;
            case 'slider':
                 return `
                    <div class="setting-group">
                        <div class="range-wrap">
                            <div class="range-head">
                                <span>${escapeHtml(widget.label)}</span>
                                <span id="${widget.display_id}">${widget.properties.value}</span>
                            </div>
                            <input type="range" id="${widget.id}" min="${widget.properties.min}" max="${widget.properties.max}" step="${widget.properties.step}" value="${widget.properties.value}">
                        </div>
                    </div>`;
            default: return '';
        }
    } catch (e) {
        console.error("Widget render error:", e);
        return `<div class="error-msg">Widget Render Error</div>`;
    }
}

// --- MODEL LIST ---
export function renderModelList(profilesData) {
    Store.currentProfile = profilesData.active_profile;
    const selector = $('modelSelector');
    if (!selector) return;
    selector.innerHTML = '';

    const groupedProfiles = {};
    for (const key in profilesData.profiles) {
        const profile = profilesData.profiles[key];
        const category = profile.category || 'Other';
        if (!groupedProfiles[category]) groupedProfiles[category] = [];
        groupedProfiles[category].push({ key, ...profile });
    }

    for (const category in groupedProfiles) {
        const group = document.createElement('optgroup');
        group.label = category;
        groupedProfiles[category].forEach(profile => {
            const option = document.createElement('option');
            option.value = profile.key;
            option.textContent = `${profile.display_name}`;
            if (profile.key === Store.currentProfile) option.selected = true;
            group.appendChild(option);
        });
        selector.appendChild(group);
    }
}

// --- MESSAGE RENDERING ---
export function addMessage(role, content) {
    const container = $('streamContainer');
    const empty = container.querySelector('.empty-void');
    if (empty) empty.remove();

    const div = document.createElement('div');
    div.className = `msg-block ${role}`;
    
    // Icon mapping
    const icon = role === 'user' ? 'user' : (role === 'system' ? 'info-circle' : 'robot');
    
    let innerHTML = `<div class="msg-avatar"><i class="fas fa-${icon}"></i></div>`;
    innerHTML += `<div class="msg-content">`;
    
    if (role === 'ai') {
        // Düşünce kutusu (Başlangıçta gizli)
        innerHTML += `<div class="thought-box" style="display:none;">
                        <div class="thought-header">
                            <span><i class="fas fa-caret-right"></i> Chain of Thought</span>
                            <span class="think-timer"></span>
                        </div>
                        <div class="thought-body"></div>
                      </div>`;
        innerHTML += `<div class="text-body">${content === '' ? '<div class="typing-indicator"><span></span><span></span><span></span></div>' : marked.parse(content)}</div>`;
    } else {
        innerHTML += `<div class="text-body">${marked.parse(content)}</div>`;
    }
    innerHTML += `</div>`;

    div.innerHTML = innerHTML;
    if (role === 'system') div.classList.add('system');
    
    container.appendChild(div);
    scrollToBottom();

    if(role === 'ai') {
        const tHeader = div.querySelector('.thought-header');
        if(tHeader) {
            tHeader.onclick = () => {
                const box = div.querySelector('.thought-box');
                box.classList.toggle('open');
            };
        }
        return {
            content: div.querySelector('.text-body'),
            thoughtBox: div.querySelector('.thought-box'),
            thoughtBody: div.querySelector('.thought-body'),
            thinkTimer: div.querySelector('.think-timer')
        };
    }
    
    return div.querySelector('.text-body');
}

// --- ARTIFACT HANDLER ---
export function updateArtifact(content) {
    // Artifact tespit mantığı: ```html ... ``` veya ```javascript ... ```
    // Basit regex ile son kod bloğunu yakala
    const matches = [...content.matchAll(/```(\w+)?\n([\s\S]*?)```/g)];
    if (matches.length > 0) {
        const lastMatch = matches[matches.length - 1];
        const lang = lastMatch[1] || 'text';
        const code = lastMatch[2];
        
        const frame = $('artifact-frame');
        const codeBlock = $('artifact-code');
        const placeholder = $('artifact-placeholder');
        
        if (placeholder) placeholder.classList.add('hidden');
        
        // Eğer HTML ise Preview yap
        if (lang === 'html') {
            frame.classList.remove('hidden');
            codeBlock.classList.add('hidden');
            frame.srcdoc = code;
        } else {
            // Değilse kod göster
            frame.classList.add('hidden');
            codeBlock.classList.remove('hidden');
            codeBlock.innerHTML = `<code class="language-${lang}">${escapeHtml(code)}</code>`;
            hljs.highlightElement(codeBlock.querySelector('code'));
        }
        
        // Paneli otomatik aç
        $('artifactPanel').classList.add('open');
    }
}

export function setPersona(key) {
    const container = $('persona-chips');
    if (!container) return;
    container.querySelectorAll('.chip').forEach(b => b.classList.remove('active'));
    const activeChip = container.querySelector(`[data-persona-key="${key}"]`);
    if(activeChip) activeChip.classList.add('active');

    const sysPrompt = $('systemPrompt');
    // Not: PERSONAS objesi state.js'den geliyor ve hala basit stringler barındırabilir.
    // Ancak backend'de default system prompt var. Burası sadece UI override için.
    if (sysPrompt && PERSONAS[key]) {
        sysPrompt.value = PERSONAS[key];
    }
}

export function updateHealthStatus(isHealthy) {
    const statusIndicator = $('modelStatusIndicator');
    if (statusIndicator) {
        statusIndicator.className = `status-dot ${isHealthy ? 'online' : (Store.isSwitching ? 'loading' : '')}`;
    }
    const statusText = $('statusText');
    if (statusText) {
        statusText.textContent = isHealthy ? 'System Online' : (Store.isSwitching ? 'Model Loading...' : 'Offline');
        statusText.style.color = isHealthy ? '#10b981' : '#f87171';
    }
}

export function togglePanel(panelId) {
    const p = $(panelId);
    if (!p) return;
    const isOpen = p.classList.contains('open');
    document.querySelectorAll('.slide-panel').forEach(pan => pan.classList.remove('open'));
    if (!isOpen) p.classList.add('open');
}

export function toggleArtifacts() {
    const p = $('artifactPanel');
    if (!p) return;
    p.classList.toggle('open');
}

export function setBusy(busy) {
    Store.generating = busy;
    const sendBtn = $('sendBtn');
    const stopBtn = $('stopBtn');
    if(sendBtn) sendBtn.style.display = busy ? 'none' : 'flex';
    if(stopBtn) stopBtn.classList.toggle('hidden', !busy);
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

export const enhanceCode = (el) => el.querySelectorAll('pre code').forEach(block => hljs.highlightElement(block));
export const scrollToBottom = () => { const el = $('streamContainer'); if(el) el.scrollTop = el.scrollHeight; };
export const clearChat = () => { 
    $('streamContainer').innerHTML = `<div class="empty-void"><div class="void-icon"><i class="fas fa-bolt"></i></div><h1>Omni-Studio v3.1</h1><p>Ready for Inference</p></div>`; 
    Store.history = []; 
};
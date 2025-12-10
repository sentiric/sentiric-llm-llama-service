import { $, Store } from './state.js';
import * as api from './api.js';
import * as ui from './ui.js';

document.addEventListener('DOMContentLoaded', initialize);

async function initialize() {
    console.log("🌌 Sentiric OS v8.1 Booting (Stateful)...");
    
    try {
        const [profilesData, layoutSchema] = await Promise.all([
            api.fetchProfiles(),
            api.fetchLayout()
        ]);
        
        const lastUsedProfile = localStorage.getItem('sentiric_last_profile');
        if (lastUsedProfile && profilesData.profiles[lastUsedProfile]) {
            // Eğer localStorage'da geçerli bir profil varsa, onu aktif et
            if (lastUsedProfile !== profilesData.active_profile) {
                console.log(`Restoring last used profile: ${lastUsedProfile}`);
                await switchModel(lastUsedProfile, profilesData.profiles[lastUsedProfile].display_name, false); // Onay sorma
            }
        }
        
        ui.renderModelList(profilesData);
        renderDynamicControls(layoutSchema);
        
        setupEventListeners();
        ui.setupCharts();
        
        await checkHealthLoop();

    } catch (error) {
        console.error("Initialization Failed:", error);
        document.body.innerHTML = `<div class="error-msg">Uygulama başlatılamadı: ${error.message}</div>`;
    }
}

function renderDynamicControls(schema) {
    const panelContainers = {
        settings: $('dynamic-controls'),
        telemetry: $('dynamic-telemetry-controls')
    };
    
    Object.keys(schema.panels).forEach(panelKey => {
        const container = panelContainers[panelKey];
        const widgets = schema.panels[panelKey];
        if (container && widgets) {
            container.innerHTML = widgets.map(ui.renderWidget).join('');
        }
    });
    
    Object.values(schema.panels).flat().forEach(widget => {
        if (widget.type === 'slider') {
            const input = $(widget.id);
            const display = $(widget.display_id);
            if (input && display) {
                input.oninput = (e) => display.innerText = e.target.value;
            }
        }
        if (widget.type === 'chip-group') {
            const container = $(widget.id);
            if (container) {
                container.querySelectorAll('.chip').forEach(chip => {
                    chip.onclick = () => ui.setPersona(chip.dataset.personaKey);
                });
            }
        }
    });
    ui.setPersona('default');
}

function setupEventListeners() {
    $('sendBtn').onclick = sendMessage;
    $('stopBtn').onclick = () => Store.controller?.abort();
    $('omniInput').addEventListener('keydown', (e) => { if (e.key === 'Enter' && !e.shiftKey) { e.preventDefault(); sendMessage(); } });
    $('toggle-stats-panel').onclick = () => ui.togglePanel('statsPanel');
    $('toggle-settings-panel').onclick = () => ui.togglePanel('settingsPanel');
    $('close-stats-panel').onclick = () => ui.togglePanel('statsPanel');
    $('close-settings-panel').onclick = () => ui.togglePanel('settingsPanel');
    $('file-upload-btn').onclick = () => $('fileInput').click();
    $('modelSelector').onchange = (e) => {
        const selectedOption = e.target.options[e.target.selectedIndex];
        switchModel(e.target.value, selectedOption.textContent.split('-')[0].trim());
    };
}

async function checkHealthLoop() {
    const { healthy } = await api.checkHealthAPI();
    ui.updateHealthStatus(healthy);
    setInterval(async () => {
        const { healthy } = await api.checkHealthAPI();
        ui.updateHealthStatus(healthy);
    }, 5000);
}

async function switchModel(profileKey, profileName, confirmUser = true) {
    if (profileKey === Store.currentProfile || Store.isSwitching) return;
    if (confirmUser && !confirm(`${profileName} profiline geçilsin mi?`)) {
        $('modelSelector').value = Store.currentProfile;
        return;
    }
    
    Store.isSwitching = true;
    ui.updateHealthStatus(false);
    $('systemOverlay').classList.remove('hidden');

    try {
        await api.switchModelAPI(profileKey);
        Store.currentProfile = profileKey;
        localStorage.setItem('sentiric_last_profile', profileKey); // Seçimi kaydet
        ui.addMessage('system', `SİSTEM: Model başarıyla **${profileName}** olarak değiştirildi.`);
    } catch (e) {
        ui.addMessage('system', `<span style="color:#ef4444">HATA: ${e.message}</span>`);
        $('modelSelector').value = Store.currentProfile;
    } finally {
        Store.isSwitching = false;
        let isHealthy = false;
        for (let i = 0; i < 20 && !isHealthy; i++) { // Timeout artırıldı
            await new Promise(resolve => setTimeout(resolve, 3000));
            isHealthy = (await api.checkHealthAPI()).healthy;
        }
        ui.updateHealthStatus(isHealthy);
        $('systemOverlay').classList.add('hidden');
    }
}

async function sendMessage() {
    const text = $('omniInput').value.trim();
    if (!text || Store.generating) return;

    $('omniInput').value = '';
    $('omniInput').style.height = 'auto';
    ui.setBusy(true);
    
    Store.history.push({role: 'user', content: text});
    ui.addMessage('user', ui.escapeHtml(text));
    
    const bubble = ui.addMessage('ai', '');
    Store.startTime = Date.now();
    Store.tokenCount = 0;
    Store.controller = new AbortController();
    
    let fullText = "";
    
    try {
        const payload = buildPayload(text);
        $('debugLog').innerText = JSON.stringify(payload, null, 2);

        const response = await api.postChatCompletions(payload, Store.controller.signal);
        const reader = response.body.getReader();
        const decoder = new TextDecoder();
        let thinkingIndicatorRemoved = false;

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
                            ui.updateStats();
                            ui.scrollToBottom();
                        }
                    } catch(e) {}
                }
            }
        }
        bubble.innerHTML = marked.parse(fullText);
        ui.enhanceCode(bubble);
        Store.history.push({role: 'assistant', content: fullText});

    } catch (e) {
        if (e.name !== 'AbortError') {
            bubble.innerHTML = `<span style="color:#ef4444">Hata: ${e.message}</span>`;
        } else {
             bubble.innerHTML = `<span style="color:#facc15">İptal edildi.</span>`;
        }
    } finally {
        ui.setBusy(false);
    }
}

function buildPayload(text) {
    const sys = $('systemPrompt')?.value || "";
    const rag = $('ragInput')?.value || "";
    const msgs = [];

    // ÖNEMLİ: Her istekte history'yi tekrar oluşturuyoruz.
    // İlk turu belirlemek için history.length'e bakmak yerine,
    // backend'deki formatter zaten bunu yapıyor. Biz sadece tüm mesajları gönderiyoruz.
    if (rag) msgs.push({ role: 'system', content: `CONTEXT:\n${rag}` });
    if (sys) msgs.push({ role: 'system', content: sys });
    
    Store.history.forEach(m => msgs.push(m));
    // Not: sendMessage'de history'ye eklediğimiz için, payload'a user mesajını burada eklemeye gerek yok.
    // Ancak temizlik için, burada bırakalım, backend'deki formatter bunu doğru yönetir.
    
    return {
        messages: msgs,
        temperature: parseFloat($('tempInput')?.value || 0.7),
        max_tokens: parseInt($('tokenLimit')?.value || 1024),
        stream: true
    };
}
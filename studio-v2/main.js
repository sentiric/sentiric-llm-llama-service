import { $, Store } from './state.js';
import * as api from './api.js';
import * as ui from './ui.js';

document.addEventListener('DOMContentLoaded', initialize);

async function initialize() {
    console.log("🌌 Sentiric Omni-Studio v3 Booting...");
    
    try {
        const [profilesData, layoutSchema] = await Promise.all([
            api.fetchProfiles(),
            api.fetchLayout()
        ]);
        
        const lastUsedProfile = localStorage.getItem('sentiric_last_profile');
        if (lastUsedProfile && profilesData.profiles[lastUsedProfile]) {
            if (lastUsedProfile !== profilesData.active_profile) {
                await switchModel(lastUsedProfile, profilesData.profiles[lastUsedProfile].display_name, false);
            }
        }
        
        ui.renderModelList(profilesData);
        renderDynamicControls(layoutSchema);
        
        setupEventListeners();
        ui.setupCharts();
        
        Store.history = [];
        
        await checkHealthLoop();

    } catch (error) {
        console.error("Initialization Failed:", error);
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
        if (widget.type === 'slider' || widget.type === 'hardware-slider') {
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
    $('omniInput').addEventListener('keydown', (e) => { 
        if (e.key === 'Enter' && !e.shiftKey) { 
            e.preventDefault(); 
            sendMessage(); 
        } 
    });
    $('toggle-stats-panel').onclick = () => ui.togglePanel('statsPanel');
    $('toggle-settings-panel').onclick = () => ui.togglePanel('settingsPanel');
    $('close-stats-panel').onclick = () => ui.togglePanel('statsPanel');
    $('close-settings-panel').onclick = () => ui.togglePanel('settingsPanel');
    
    // Artifact Toggle
    $('toggle-artifacts').onclick = () => ui.toggleArtifacts();
    $('closeArtifact').onclick = () => ui.toggleArtifacts();

    $('file-upload-btn').onclick = () => $('fileInput').click();
    
    $('modelSelector').onchange = (e) => {
        const selectedOption = e.target.options[e.target.selectedIndex];
        switchModel(e.target.value, selectedOption.textContent.split('-')[0].trim());
    };

    // Hardware Update Button
    const hwBtn = $('applyHardwareBtn');
    if(hwBtn) hwBtn.onclick = applyHardwareConfig;
}

async function applyHardwareConfig() {
    if (!confirm("Donanım ayarlarını güncellemek modeli yeniden yükleyecektir. Devam edilsin mi?")) return;
    
    const layers = parseInt($('gpuLayers')?.value || -1);
    const ctx = parseInt($('ctxSize')?.value || 4096);
    
    ui.setBusy(true);
    $('systemOverlay').classList.remove('hidden');
    $('overlayTitle').innerText = "Donanım Yapılandırılıyor";

    try {
        const res = await fetch('/v1/hardware/config', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({ gpu_layers: layers, context_size: ctx, kv_offload: true })
        });
        
        if (res.ok) {
            alert("Donanım ayarları başarıyla güncellendi.");
        } else {
            const err = await res.json();
            alert("Hata: " + (err.message || "Bilinmeyen hata"));
        }
    } catch(e) {
        alert("Bağlantı hatası: " + e.message);
    } finally {
        $('systemOverlay').classList.add('hidden');
        ui.setBusy(false);
    }
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

    Store.history = [];
    document.getElementById('streamContainer').innerHTML = '';
    
    try {
        await api.switchModelAPI(profileKey);
        Store.currentProfile = profileKey;
        localStorage.setItem('sentiric_last_profile', profileKey);
        ui.addMessage('system', `SİSTEM: Model başarıyla **${profileName}** olarak değiştirildi.`);
    } catch (e) {
        ui.addMessage('system', `<span style="color:#ef4444">HATA: ${e.message}</span>`);
        $('modelSelector').value = Store.currentProfile;
    } finally {
        Store.isSwitching = false;
        let isHealthy = false;
        for (let i = 0; i < 20 && !isHealthy; i++) {
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
    
    ui.addMessage('user', ui.escapeHtml(text));
    Store.history.push({role: 'user', content: text});

    const uiElements = ui.addMessage('ai', '');
    const bubble = uiElements.content;
    const thoughtBox = uiElements.thoughtBox;
    const thoughtBody = uiElements.thoughtBody;
    const thinkTimer = uiElements.thinkTimer;

    Store.startTime = Date.now();
    Store.tokenCount = 0;
    Store.controller = new AbortController();
    
    let fullText = "";
    let fullThought = "";
    let isThinking = false;
    let thinkingIndicatorRemoved = false;
    let thinkingStartTime = 0;
    let totalThinkingTime = 0;
    
    try {
        const payload = buildPayload(); 
        $('debugLog').innerText = JSON.stringify(payload, null, 2);

        const response = await api.postChatCompletions(payload, Store.controller.signal);
        const reader = response.body.getReader();
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
                        let content = json.choices[0]?.delta?.content;
                        
                        if (content) {
                            if (!thinkingIndicatorRemoved) {
                                bubble.innerHTML = '';
                                thinkingIndicatorRemoved = true;
                            }

                            if (!isThinking && (content.match(/<think>/i) || content.match(/<thought>/i))) {
                                isThinking = true;
                                thinkingStartTime = Date.now();
                                thoughtBox.style.display = 'block';
                                content = content.replace(/<think>|<thought>/gi, '');
                            }

                            if (isThinking && (content.match(/<\/think>/i) || content.match(/<\/thought>/i))) {
                                isThinking = false;
                                
                                const parts = content.split(/<\/think>|<\/thought>/i);
                                if (parts.length >= 2) {
                                    fullThought += parts[0];
                                    thoughtBody.innerText = fullThought;
                                    content = parts[1];
                                } else {
                                    content = content.replace(/<\/think>|<\/thought>/gi, '');
                                }
                                
                                totalThinkingTime += (Date.now() - thinkingStartTime);
                                thinkTimer.innerText = `(${(totalThinkingTime/1000).toFixed(1)}s)`;
                            }

                            Store.tokenCount++;

                            if (isThinking) {
                                fullThought += content;
                                thoughtBody.innerText = fullThought;
                                const currentThinkTime = (Date.now() - thinkingStartTime);
                                thinkTimer.innerText = `(${((currentThinkTime)/1000).toFixed(1)}s)`;
                            } else {
                                fullText += content;
                                bubble.innerHTML = marked.parse(fullText + '▋');
                                // ARTIFACT LIVE PREVIEW
                                ui.updateArtifact(fullText);
                            }
                            
                            ui.updateStats();
                            ui.scrollToBottom();
                        }
                    } catch(e) {}
                }
            }
        }
        
        bubble.innerHTML = marked.parse(fullText);
        ui.enhanceCode(bubble);
        ui.updateArtifact(fullText); // Final check
        Store.history.push({role: 'assistant', content: fullText});

    } catch (e) {
        if (e.name !== 'AbortError') {
            bubble.innerHTML = `<span style="color:#ef4444">Hata: ${e.message}</span>`;
            Store.history.pop();
        } else {
             bubble.innerHTML += `<span style="color:#facc15"> [İptal edildi]</span>`;
        }
    } finally {
        ui.setBusy(false);
    }
}

function buildPayload() {
    const sys = $('systemPrompt')?.value || "";
    const rag = $('ragInput')?.value || "";
    const msgs = [];

    const reasoningEl = document.querySelector('input[name="reasoning-level"]:checked');
    const reasoningLevel = reasoningEl ? reasoningEl.value : "none";

    let finalSystemPrompt = sys;
    if (rag) {
        finalSystemPrompt = `CONTEXT:\n${rag}\n\nINSTRUCTIONS:\n${sys}`;
    }
    
    if (finalSystemPrompt) {
        msgs.push({ role: 'system', content: finalSystemPrompt });
    }
    
    Store.history.forEach(m => msgs.push(m));
    
    return {
        messages: msgs,
        temperature: parseFloat($('tempInput')?.value || 0.7),
        max_tokens: parseInt($('tokenLimit')?.value || 1024),
        reasoning_effort: reasoningLevel, 
        stream: true
    };
}
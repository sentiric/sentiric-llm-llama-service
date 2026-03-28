// Ortak başlıkları (Headers) hazırlayan yardımcı fonksiyon
function getHeaders() {
    return {
        'Content-Type': 'application/json',
        //[ARCH-COMPLIANCE] Strict Tenant Isolation ve Tracing gereksinimleri
        'x-tenant-id': 'sentiric_studio', 
        'x-trace-id': 'ui-req-' + Date.now()
    };
}

export async function fetchProfiles() {
    const res = await fetch('/v1/profiles', { headers: getHeaders() });
    if (!res.ok) throw new Error('Profiller alınamadı.');
    return res.json();
}

export async function fetchLayout() {
    const res = await fetch('/v1/ui/layout', { headers: getHeaders() });
    if (!res.ok) throw new Error('UI şeması alınamadı.');
    return res.json();
}

export async function switchModelAPI(profileKey) {
    const res = await fetch('/v1/models/switch', {
        method: 'POST',
        headers: getHeaders(),
        body: JSON.stringify({ profile: profileKey })
    });
    if (!res.ok) {
        const errData = await res.json();
        throw new Error(errData.message || 'Model değiştirilemedi.');
    }
    return res.json();
}

export async function checkHealthAPI() {
    try {
        const res = await fetch('/health', { headers: getHeaders() });
        if (!res.ok) return { healthy: false };
        const data = await res.json();
        return { healthy: data.status === 'healthy' };
    } catch {
        return { healthy: false };
    }
}

export async function postChatCompletions(payload, signal) {
    const res = await fetch('/v1/chat/completions', {
        method: 'POST',
        headers: getHeaders(),
        body: JSON.stringify(payload),
        signal: signal
    });
    
    if (!res.ok) {
        // [FIX] Backend'den dönen SUTS JSON hatasını daha zarif okuma
        let errorMsg = `API Hatası: ${res.statusText}`;
        try {
            const errObj = await res.json();
            if (errObj.error && errObj.error.message) {
                errorMsg = errObj.error.message;
            }
        } catch (e) { /* Parse edilemezse varsayılanı kullan */ }

        throw new Error(res.status === 503 ? "Model meşgul veya yükleniyor..." : errorMsg);
    }
    return res;
}
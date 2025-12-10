export async function fetchProfiles() {
    const res = await fetch('/v1/profiles');
    if (!res.ok) throw new Error('Profiller alınamadı.');
    return res.json();
}

export async function fetchLayout() {
    const res = await fetch('/v1/ui/layout');
    if (!res.ok) throw new Error('UI şeması alınamadı.');
    return res.json();
}

export async function switchModelAPI(profileKey) {
    const res = await fetch('/v1/models/switch', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
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
        const res = await fetch('/health');
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
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify(payload),
        signal: signal
    });
    if (!res.ok) {
        throw new Error(res.status === 503 ? "Model meşgul veya yükleniyor..." : `API Hatası: ${res.statusText}`);
    }
    return res;
}
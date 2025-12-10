export const $ = (id) => document.getElementById(id);

export const Store = {
    generating: false,
    controller: null,
    history: [],
    startTime: 0,
    tokenCount: 0,
    currentProfile: "sentiric_6gb_qwen25_3b",
    isSwitching: false,
    chart: null,
    chartData: { labels: Array(30).fill(''), data: Array(30).fill(0) }
};

export const PERSONAS = {
    'default': "Sen yardımsever bir asistansın. Kısa, net ve Türkçe cevaplar ver.",
    'coder': "Sen uzman bir yazılım mühendisisin. Temiz, etkili ve iyi belgelenmiş kodlar sun.",
    'creative': "Sen yaratıcı bir yazarsın. Hikayeler, şiirler ve senaryolar üretebilirsin.",
    'english': "You are a helpful assistant. Respond in English."
};
const mem = new Map();
const STORAGE_KEY = 'obs17live_webp_durations_v1';

function canUseStorage() {
  return typeof window !== 'undefined' && window.localStorage;
}

function loadFromStorage(src) {
  if (!canUseStorage()) return null;
  try {
    const raw = window.localStorage.getItem(STORAGE_KEY);
    if (!raw) return null;
    const map = JSON.parse(raw);
    const v = map && typeof map === 'object' ? map[src] : null;
    return typeof v === 'number' && Number.isFinite(v) && v > 0 ? v : null;
  } catch {
    return null;
  }
}

function saveToStorage(src, durationMs) {
  if (!canUseStorage()) return;
  try {
    const raw = window.localStorage.getItem(STORAGE_KEY);
    const map = raw ? JSON.parse(raw) : {};
    if (!map || typeof map !== 'object') return;
    map[src] = durationMs;
    window.localStorage.setItem(STORAGE_KEY, JSON.stringify(map));
  } catch {
    return;
  }
}

export async function getWebpDurationMs(src) {
  if (!src || typeof src !== 'string') return null;
  const normalizedSrc = src.split('?')[0].toLowerCase();
  if (!normalizedSrc.endsWith('.webp')) return null;

  const cached = mem.get(src);
  if (typeof cached === 'number') return cached;
  if (cached && typeof cached.then === 'function') return cached;

  const fromStorage = loadFromStorage(src);
  if (fromStorage) {
    mem.set(src, fromStorage);
    return fromStorage;
  }

  const promise = (async () => {
    try {
      if (typeof ImageDecoder === 'undefined') return null;
      const res = await fetch(src);
      if (!res.ok) return null;
      const buf = await res.arrayBuffer();
      const decoder = new ImageDecoder({ data: buf, type: 'image/webp' });
      await decoder.tracks.ready;
      const track = decoder.tracks.selectedTrack;
      const count = track && typeof track.frameCount === 'number' ? track.frameCount : 0;
      if (!count) return null;
      let totalUs = 0;
      for (let i = 0; i < count; i++) {
        const r = await decoder.decode({ frameIndex: i });
        const img = r && r.image;
        const d = img && typeof img.duration === 'number' ? img.duration : 0;
        if (d > 0) totalUs += d;
        if (img && typeof img.close === 'function') img.close();
      }
      if (!totalUs) return null;
      const ms = Math.round(totalUs / 1000);
      if (!Number.isFinite(ms) || ms <= 0) return null;
      saveToStorage(src, ms);
      return ms;
    } catch {
      return null;
    }
  })();

  mem.set(src, promise);
  const v = await promise;
  mem.set(src, v || null);
  return v || null;
}

let cached = null;
let pendingRequest = null;

function sanitizeI18nConfig(raw) {
  if (!raw || typeof raw !== 'object' || Array.isArray(raw)) return raw;
  const next = { ...raw };
  delete next.__17live_language;
  return next;
}

async function loadMockI18nConfig() {
  if (typeof window === 'undefined') return null;
  if (cached) return cached;

  try {
    const res = await fetch('/mock/get_i18n_response.json');
    if (!res.ok) return null;
    cached = sanitizeI18nConfig(await res.json());
    return cached;
  } catch {
    return null;
  }
}

if (process.env.NODE_ENV === 'development' && typeof window !== 'undefined') {
  loadMockI18nConfig();
}

export async function getI18nConfig() {
  if (cached) return cached;
  if (pendingRequest) return pendingRequest;

  if (process.env.NODE_ENV === 'development') {
    const mock = await loadMockI18nConfig();
    if (mock) return mock;
  }

  pendingRequest = (async () => {
    const res = await fetch('/lapi', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ action: 'getI18nConfig' }),
    });

    if (!res.ok) {
      throw new Error(`Failed to fetch i18n config: ${res.status}`);
    }

    cached = sanitizeI18nConfig(await res.json());
    return cached;
  })();

  try {
    return await pendingRequest;
  } finally {
    pendingRequest = null;
  }
}

export async function getI18nValueByKey(key) {
  if (!key) return '';
  const config = await getI18nConfig();
  if (!config || typeof config !== 'object') return '';
  const value = config[key];
  return typeof value === 'string' ? value : '';
}

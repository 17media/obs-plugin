let cached = null;

async function loadMockI18nConfig() {
  if (typeof window === 'undefined') return null;
  if (cached) return cached;

  try {
    const res = await fetch('/mock/get_i18n_response.json');
    if (!res.ok) return null;
    cached = await res.json();
    return cached;
  } catch {
    return null;
  }
}

export async function getI18nConfig() {
  if (cached) return cached;

  if (process.env.NODE_ENV === 'development') {
    const mock = await loadMockI18nConfig();
    if (mock) return mock;
  }

  const res = await fetch('/lapi', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ action: 'getI18nConfig' }),
  });

  if (!res.ok) {
    throw new Error(`Failed to fetch i18n config: ${res.status}`);
  }

  cached = await res.json();
  return cached;
}


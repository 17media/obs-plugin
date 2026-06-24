let cached = null;

async function loadMockEnterAnimationFiles() {
  if (typeof window === 'undefined') return null;
  if (cached) return cached;

  const candidates = [
    '/mock/get_files_list_response.json',
    '/mock/get_enter_animation_files_list_response.json',
  ];

  for (const url of candidates) {
    try {
      const res = await fetch(url);
      if (!res.ok) continue;
      const json = await res.json();
      cached = json;
      return json;
    } catch {
      continue;
    }
  }

  return null;
}

export async function getEnterAnimationFiles() {
  if (process.env.NODE_ENV === 'development') {
    const mock = await loadMockEnterAnimationFiles();
    if (mock) return mock;
  }

  const url = `/lapi`;
  const data = {
    action: 'getEnterAnimationFiles',
  };

  const res = await fetch(url, {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json',
    },
    body: JSON.stringify(data),
  });

  if (!res.ok) {
    throw new Error(`Failed to fetch enter animation files: ${res.status}`);
  }

  return res.json();
}

let cached = null;
let pendingRequest = null;

function shouldLogEnterAnimationFilesDebug() {
  if (typeof window === 'undefined') return process.env.NODE_ENV === 'development';
  try {
    const params = new URLSearchParams(window.location.search);
    const v =
      params.get('logAnim14') ||
      params.get('debugAnim14') ||
      params.get('logEnterAnimationFiles') ||
      '';
    return (
      process.env.NODE_ENV === 'development' ||
      v === '1' ||
      v.toLowerCase() === 'true' ||
      v.toLowerCase() === 'yes'
    );
  } catch {
    return process.env.NODE_ENV === 'development';
  }
}

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
  if (cached) return cached;
  if (pendingRequest) return pendingRequest;

  if (process.env.NODE_ENV === 'development') {
    const mock = await loadMockEnterAnimationFiles();
    if (mock) return mock;
  }

  const url = `/lapi`;
  const data = {
    action: 'getEnterAnimationFiles',
  };

  // if (shouldLogEnterAnimationFilesDebug()) {
  //   console.log('[enter_animation][files] request start', {
  //     url,
  //     action: data.action,
  //     hasCachedMock: Boolean(cached),
  //     href: typeof window !== 'undefined' ? window.location.href : '',
  //   });
  // }

  pendingRequest = (async () => {
    const res = await fetch(url, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
      },
      body: JSON.stringify(data),
    });

    if (!res.ok) {
      // if (shouldLogEnterAnimationFilesDebug()) {
      //   console.warn('[enter_animation][files] request failed', {
      //     url,
      //     action: data.action,
      //     status: res.status,
      //     statusText: res.statusText,
      //   });
      // }
      throw new Error(`Failed to fetch enter animation files: ${res.status}`);
    }

    const json = await res.json();
    cached = json;
    // if (shouldLogEnterAnimationFilesDebug()) {
    //   const files = Array.isArray(json?.files)
    //     ? json.files
    //     : Array.isArray(json?.animations)
    //       ? json.animations
    //       : [];
    //   console.log('[enter_animation][files] request success', {
    //     url,
    //     action: data.action,
    //     status: res.status,
    //     fileCount: files.length,
    //     sampleIds: files.slice(0, 5).map((f) => f?.animationID || f?.animationId || f?.id || f?.name || null),
    //   });
    // }

    return json;
  })();

  try {
    return await pendingRequest;
  } finally {
    pendingRequest = null;
  }
}

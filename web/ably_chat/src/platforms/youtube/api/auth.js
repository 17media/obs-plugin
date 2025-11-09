// YouTube token retrieval
// - development: read from env `NEXT_PUBLIC_YOUTUBE_TOKEN`
// - production: fetch from REST API `/token/youtube`

export async function getYouTubeToken() {
  if (process.env.NODE_ENV === 'development') {
    const token = process.env.NEXT_PUBLIC_YOUTUBE_TOKEN;
    if (!token) {
      throw new Error('NEXT_PUBLIC_YOUTUBE_TOKEN is not set in development environment');
    }
    return token.trim();
  }

  try {
    const res = await fetch('/token/youtube', { method: 'GET' });
    if (!res.ok) {
      throw new Error(`Failed to fetch YouTube token: ${res.status}`);
    }
    const contentType = res.headers.get('content-type');
    let token = '';
    if (contentType && contentType.includes('application/json')) {
      const data = await res.json();
      token = data?.token || data?.access_token || '';
    } else {
      token = (await res.text())?.trim();
    }
    if (!token) {
      throw new Error('Invalid /token/youtube response: missing token');
    }
    return token;
  } catch (err) {
    console.error('Error retrieving YouTube token:', err);
    throw err;
  }
}
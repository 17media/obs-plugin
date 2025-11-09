// Twitch token retrieval
// - development: read from env `NEXT_PUBLIC_TWITCH_TOKEN`
// - production: fetch from REST API `/token/twitch`

export async function getTwitchToken() {
  if (process.env.NODE_ENV === 'development') {
    const token = process.env.NEXT_PUBLIC_TWITCH_TOKEN;
    if (!token) {
      throw new Error('NEXT_PUBLIC_TWITCH_TOKEN is not set in development environment');
    }
    return token.startsWith('oauth:') ? token : `oauth:${token}`;
  }

  try {
    const res = await fetch('/token/twitch', { method: 'GET' });
    if (!res.ok) {
      throw new Error(`Failed to fetch Twitch token: ${res.status}`);
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
      throw new Error('Invalid /token/twitch response: missing token');
    }
    return token.startsWith('oauth:') ? token : `oauth:${token}`;
  } catch (err) {
    console.error('Error retrieving Twitch token:', err);
    throw err;
  }
}
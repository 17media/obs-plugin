// Get WebSocket server URL depending on environment
// - development: read from NEXT_PUBLIC_WS_URL
// - production: fetch from '/ws-url'

export async function getWebSocketServerURL() {
    if (process.env.NODE_ENV === 'development') {
        const url = process.env.NEXT_PUBLIC_WS_URL;
        if (!url) {
            throw new Error('NEXT_PUBLIC_WS_URL is not set in development environment');
        }
        return url;
    }

    try {
        const res = await fetch('/ws-url');
        if (!res.ok) {
            throw new Error(`Failed to fetch WebSocket URL: ${res.status}`);
        }

        const contentType = res.headers.get('content-type');
        let url = '';
        if (contentType && contentType.includes('application/json')) {
            const data = await res.json();
            url = data?.url || data?.wsUrl || data?.WS_URL || data?.ws_url || '';
        } else {
            url = (await res.text())?.trim();
        }

        if (!url) {
            throw new Error('Invalid /ws-url response: missing URL');
        }

        return url;
    } catch (err) {
        console.error('Error retrieving WebSocket URL:', err);
        throw err;
    }
}
'use client'

import { useEffect } from 'react'
import * as Ably from 'ably'
import { getAblyTokenFromServer } from '../auth'

export default function Relay() {
  useEffect(() => {
    let ably
    let channel
    let ws
    let queue = []
    let reconnectAttempts = 0
    const baseDelay = 1000
    let isClosing = false
    const params = new URLSearchParams(window.location.search)
    const roomID = params.get('roomID') || ''
    const wsUrl = params.get('ws') ? decodeURIComponent(params.get('ws')) : ''
    console.log('[Relay] init params', { roomID, wsUrl })
    if (!roomID || !wsUrl) {
      console.error('[Relay] missing params: roomID or ws')
      return
    }
    const flush = () => {
      if (!ws || ws.readyState !== WebSocket.OPEN) return
      while (queue.length && ws.readyState === WebSocket.OPEN) {
        const msg = queue.shift()
        try { ws.send(msg) } catch { queue.unshift(msg); break }
      }
    }

    const send = (payload) => {
      const data = typeof payload === 'string' ? payload : JSON.stringify(payload)
      if (ws && ws.readyState === WebSocket.OPEN) {
        try {
          // console.log('[Relay] WS send', data)
          ws.send(data); return true
        } catch (e) {
          console.error('[Relay] WS send error', e)
          return false
        }
      }
      if (ws && (ws.readyState === WebSocket.CONNECTING || ws.readyState === WebSocket.CLOSING)) {
        queue.push(data)
        console.log('[Relay] WS queue push, length', queue.length)
      }
      return false
    }

    const scheduleReconnect = () => {
      if (isClosing) return
      reconnectAttempts = Math.min(reconnectAttempts + 1, 10)
      const delay = Math.min(baseDelay * Math.pow(2, reconnectAttempts), 15000)
      console.log('[Relay] WS scheduleReconnect attempt', reconnectAttempts + 1)
      setTimeout(() => {
        try { ws = new WebSocket(wsUrl) } catch { scheduleReconnect(); return }
        ws.onopen = () => {
          reconnectAttempts = 0
          send({ type: 'action', payload: { type: 'register_chatdock' } })
          send({ type: 'ably_chat_connected', payload: { roomID } })
          flush()
        }
        ws.onerror = (e) => { console.error('[Relay] WS error', e) }
        ws.onclose = (e) => { console.warn('[Relay] WS close', e?.code, e?.reason); if (!isClosing) scheduleReconnect() }
      }, delay)
    }

    try {
      ws = new WebSocket(wsUrl)
      ws.onopen = () => {
        reconnectAttempts = 0
        send({ type: 'action', payload: { type: 'register_chatdock' } })
        send({ type: 'ably_chat_connected', payload: { roomID } })
        flush()
      }
      ws.onerror = (e) => { console.error('[Relay] WS error', e) }
      ws.onclose = (e) => { console.warn('[Relay] WS close', e?.code, e?.reason); if (!isClosing) scheduleReconnect() }
    } catch { scheduleReconnect() }
    ably = new Ably.Realtime({
      environment: '17media',
      fallbackHosts: [
        '17-media-a-fallback.ably-realtime.com',
        '17-media-b-fallback.ably-realtime.com',
        '17-media-c-fallback.ably-realtime.com'
      ],
      authCallback: async (_, cb) => {
        try {
          const token = await getAblyTokenFromServer(roomID)
          cb(null, token)
        } catch (e) {
          cb(e, null)
        }
      }
    })
    ably.connection.on('connecting', () => console.log('[Relay] Ably connecting'))
    ably.connection.on('connected', () => console.log('[Relay] Ably connected'))
    ably.connection.on('disconnected', () => console.warn('[Relay] Ably disconnected'))
    ably.connection.on('suspended', () => console.warn('[Relay] Ably suspended'))
    ably.connection.on('closed', () => console.warn('[Relay] Ably closed'))
    ably.connection.on('failed', (e) => console.error('[Relay] Ably failed', e))
    channel = ably.channels.get(roomID)
    channel.subscribe((message) => {
      // console.log('[Relay] Ably message', { data: message.data })
      const payload = { type: 'ably_chat_message', payload: { roomID, data: message.data } }
      send(payload)
    })
    return () => {
      isClosing = true
      console.log('[Relay] cleanup')
      try { channel && channel.unsubscribe() } catch { }
      try { ably && ably.close() } catch { }
      try { ws && ws.close() } catch { }
    }
  }, [])
  return null
}

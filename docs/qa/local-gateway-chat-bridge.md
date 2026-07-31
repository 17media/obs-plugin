# Local Gateway + Chat Bridge

This document describes how the plugin wires the local WebSocket server to the ChatDock frontend,
including registration, disconnect cleanup, and queued event flush behavior.

References:

- Protocol: [local-protocol.md](file:///Users/zhuyu/workspace/mk/17live/dev/obs-17live/docs/local-protocol.md)
- Wiring: [OneSevenLiveCoreManager.cpp](file:///Users/zhuyu/workspace/mk/17live/dev/obs-17live/src/17live/OneSevenLiveCoreManager.cpp)
- Server: [OneSevenLiveWebsocketServer.cpp](file:///Users/zhuyu/workspace/mk/17live/dev/obs-17live/src/17live/websocket/OneSevenLiveWebsocketServer.cpp)
- Bridge: [ChatBridgeService.cpp](file:///Users/zhuyu/workspace/mk/17live/dev/obs-17live/src/17live/core/ChatBridgeService.cpp)

## Sequence (Register + Flush)

```mermaid
sequenceDiagram
  participant OBS as OBS/Plugin
  participant LG as LocalGatewayService
  participant WS as OneSevenLiveWebsocketServer
  participant CB as ChatBridgeService
  participant WEB as ChatDock (web/ably_chat)

  OBS->>LG: initLocalServers()
  LG->>WS: start(host=localhost, port=0)
  OBS->>WS: setMessageCallback / setConnectionCallback

  Note over WEB: Connects only if ws= is present in URL
  WEB->>WS: WebSocket connect
  WS->>CB: onWebsocketConnectionChanged(clientId, connected=true)
  CB->>CB: flushChatEventQueue() (no-op unless registered)

  WEB->>WS: {"type":"action","payload":{"type":"register_chatdock"}}
  WS->>CB: onWebsocketMessage(clientId, message)
  CB->>CB: chatDockClientId_=clientId
  CB->>CB: flushChatEventQueue()
  loop for each queued message
    CB->>WS: sendMessageToClient(chatDockClientId, WsMessage.dump())
  end
```

## Sequence (Disconnect + Reconnect + Flush)

```mermaid
sequenceDiagram
  participant WS as OneSevenLiveWebsocketServer
  participant CB as ChatBridgeService
  participant WEB as ChatDock (web/ably_chat)

  WEB--x WS: disconnect / close
  WS->>CB: onWebsocketConnectionChanged(clientId, connected=false)
  CB->>CB: if clientId==chatDockClientId_ then clear chatDockClientId_

  Note over CB: Chat events continue to enqueue while no chatDockClientId_ is available

  WEB->>WS: reconnect
  WS->>CB: onWebsocketConnectionChanged(newClientId, connected=true)
  CB->>CB: flushChatEventQueue() (still no-op until register)
  WEB->>WS: {"type":"action","payload":{"type":"register_chatdock"}}
  WS->>CB: onWebsocketMessage(newClientId, message)
  CB->>CB: chatDockClientId_=newClientId
  CB->>CB: flushChatEventQueue()
```

## Regression Focus

- Disconnect cleanup: chatDockClientId_ is cleared only when the registered client disconnects.
- Reconnect behavior: connection alone does not register; register message is required.
- Queue semantics:
  - When not registered, events are enqueued up to a max size and older items drop first.
  - When registered, events are sent directly and queue drains on flush.


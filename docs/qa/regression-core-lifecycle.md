# Core Lifecycle Regression Checklist

This checklist validates that the refactors around CoreRuntime/AuthSessionService/LocalGatewayService/ChatBridgeService/DockOrchestrator keep behavior unchanged.

## Preconditions

- OBS starts successfully and loads the plugin.
- Build configuration: `macos-user` preset (or equivalent).
- Network access to 17Live API endpoints if login/stream actions are validated.

## Smoke

- Start OBS and confirm plugin loads without crash.
- Open OBS, wait 10 seconds, then close OBS; confirm no hang on quit.

## Local Servers + ChatDock

- Open Chat Room dock from the menu.
- Verify the page loads and connects to local WS (ChatDock registers).
- Close Chat Room dock (dock should hide, not destroy).
- Reopen Chat Room dock, verify it reconnects and continues receiving events.

Local gateway + chat bridge behavior reference:

- [local-gateway-chat-bridge.md](file:///Users/zhuyu/workspace/mk/17live/dev/obs-17live/docs/qa/local-gateway-chat-bridge.md)

## Dock Lifecycle

- Toggle each dock from the menu:
  - Streaming
  - Live List
  - Rock Zone
  - Multi-RTMP
  - Preview
  - Chat Room
- Close all docks (via plugin shutdown or manual close of OBS).
- Restart OBS and verify previously visible docks restore correctly when logged in.

## Login / Logout

- Login successfully:
  - Verify menu shows logged-in user name.
  - Verify Streaming dock can be opened.
- Logout when not streaming:
  - Verify docks close.
  - Verify local tokens/config are cleared as expected.
- Logout while streaming:
  - Confirm warning dialog appears.
  - Confirm choosing “No” keeps session intact.
  - Confirm choosing “Yes” stops streaming and then logs out.

## Chat Queue Behavior

- With Chat Room dock closed:
  - Trigger chat events (YouTube/Twitch/Ably) and confirm no crash.
  - Reopen Chat Room dock and confirm queued events flush.
- With Chat Room dock open:
  - Verify events are delivered without noticeable delay.
- Close OBS while chat events are flowing and confirm clean shutdown.

- Disconnect → reconnect → flush:
  - Close Chat Room dock or force WS disconnect, then trigger chat events (queue grows).
  - Reconnect Chat Room dock and confirm register happens and queued events flush.

## Shutdown Idempotency

- Close OBS immediately after startup (no login).
- Close OBS after opening multiple docks.
- Close OBS during login flow (login dialog open).
- Close OBS while streaming (auto-close / manual close paths).

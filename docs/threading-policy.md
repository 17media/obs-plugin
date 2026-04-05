# Threading Policy

## Goals

- Prefer a single concurrency stack to simplify shutdown, observability, and debugging.
- Ensure all background work is either cancellable or bounded by a timeout during shutdown.
- Avoid tasks touching destroyed objects after plugin unload.

## Allowed Primitives

- Short-lived background work: `ScheduleOBSTask(...)` (plugin-owned `QThreadPool`).
- Periodic work: `QTimer` on the Qt main thread where feasible.
- Long-running blocking loops (servers, persistent connections): `QThread` (typically via `QThread::create(...)`) plus an explicit `stop()` API.

## Not Allowed

- New `std::thread` usage in plugin code. Use Qt primitives instead.

## Shutdown Rules

- Plugin unload path must wait for all queued background tasks:
  - `DestroyThreadPool()` acts as the barrier for `ScheduleOBSTask(...)`.
- Any component that starts a `QThread` must:
  - Provide `stop()` that unblocks the thread (e.g., `svr_.stop()`, `server_->stop()`).
  - Call `wait(timeoutMs)` and clear thread pointers during shutdown/destruction.
- UI objects are destroyed on the UI thread (use `deleteLater()` when needed).

## Observability

- Set a meaningful `objectName` on `QThread` instances created by the plugin.

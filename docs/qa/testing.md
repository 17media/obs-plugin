# Testing

## Unit Tests

Unit tests are built as a separate executable and registered with CTest.

Configure with tests enabled:

```bash
cmake --preset macos -DENABLE_TESTS=ON
```

Build:

```bash
cmake --build --preset macos --config RelWithDebInfo
```

Run:

```bash
ctest --test-dir build_macos -C RelWithDebInfo
```

Current coverage (minimum set):

- `WsMessage` parse/dump behavior
- `CoreRuntime` initialize/shutdown idempotency
- `Result<T>`/`Result<void>` success/error semantics
- Chat queue trimming/order (`WsMessageQueue`)

## Manual Smoke

See [regression-core-lifecycle.md](file:///Users/zhuyu/workspace/mk/17live/dev/obs-17live/docs/qa/regression-core-lifecycle.md).

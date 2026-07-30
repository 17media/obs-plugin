# Error Handling

## Goals

- Make failures diagnosable: every failure has a stable code and context detail.
- Keep responsibilities clear: lower layers produce structured errors; upper layers decide logging/UI.
- Reduce mixed styles (`bool` / exceptions / optional / string side-channels).

## Result<T>

Use `Result<T>` (and `Result<void>`) for operations that can fail and need error context.

Error fields:

- `code`: stable identifier (e.g. `Network.Timeout`, `Api.Unauthorized`, `Json.ParseFailed`)
- `message`: short human-readable message for logs
- `retryable`: whether a retry might succeed without user action
- `detail`: extra context (endpoint, http status, raw snippet, step name); safe to log

Guidelines:

- Do not throw across module boundaries; catch at external-library boundaries or thread entry points,
  then convert to `Result<T>`.
- Use `std::optional` only for “value is legitimately absent”; not for errorable operations.
- Do not rely on string side-channels (e.g. lastErrorMessage) as the primary carrier of errors.
  When legacy APIs must keep `bool`, they should also expose `ResultError`.

## Layering

- Low-level (IO/HTTP/JSON): returns `Result<T>`; never shows UI; logs only when needed for
  diagnostics at the boundary.
- Service/core/workflow: composes multiple `Result<T>` steps; may aggregate errors; logs at
  “operation level” (start/end/failure) with codes and context.
- UI: maps `ResultError` to localized strings and decides dialogs/toasts; avoid showing raw `detail`
  to users.

## Codes

Recommended prefix groups:

- `Api.*`: API gateway/business errors
- `Auth.*`: authentication/session errors
- `Network.*`: connectivity/timeout/TLS errors
- `IO.*`: filesystem/permissions/path errors
- `Json.*`: parse/validation errors
- `State.*`: invalid state/contract violations
- `Cancelled.*`: user/shutdown cancellations

## Migration Approach

1. Introduce `Result<T>` in `src/17live/utility/Result.hpp`.
2. For legacy `bool` APIs, keep the signature but store structured `ResultError` for callers.
3. Update key workflows to collect per-step errors (instead of “catch+log+bool”), then build a
   single user-facing message at the top.
4. Gradually move remaining modules to `Result<T>` as they are touched.

## Project Examples

- API wrapper (legacy `bool`):
  - Exposes `getLastError()` returning `ResultError` and keeps `getLastErrorMessage()` for UI.
- Config manager (legacy `bool`):
  - On `getConfigValue/getConfig/setConfig` failures, sets `ResultError` with IO/JSON/state codes.
- Meta loader (legacy `bool`):
  - `LoadMetaData()` sets a global last error retrievable via `GetLastMetaError()` for startup logs.

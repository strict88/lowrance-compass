# Contract: WebSocket `/ws`

One socket, server-push only (the UI issues commands over REST, per `contracts/rest-api.md`, not
over the socket — keeps the command path testable with plain HTTP and the live-data path simple).
Every message is a JSON object with a `type` and `schema`, so clients/tests can ignore types they
don't handle instead of breaking.

Broadcast rate is bounded per `data-model.md` §1.5 (`WS_STATUS_RATE_HZ_IDLE` /
`WS_STATUS_RATE_HZ_ACTIVE_CAL`) so it can never starve `ImuTask`/`N2kTask` (constitution Principle
III). A slow/stalled client is dropped rather than allowed to back-pressure the broadcast.

## `status` (periodic)

Same shape as `GET /api/status` (`contracts/rest-api.md`), sent at `WS_STATUS_RATE_HZ_IDLE` when no
calibration is active. This is the only message type most UI views need.

```json
{ "type": "status", "schema": 1, "...": "same fields as GET /api/status" }
```

## `calibration_progress` (only while a session is active)

Sent at `WS_STATUS_RATE_HZ_ACTIVE_CAL`, in addition to `status`, so the progress ring / live
indicators update smoothly without polling.

```json
{ "type": "calibration_progress", "schema": 1, "stage": "A" | "B" | "C", "sub_state": "...", "progress": { "...": "see contracts/rest-api.md session.progress shapes" } }
```

## `calibration_result` (once, on entering a result-ready or terminal state)

```json
{ "type": "calibration_result", "schema": 1, "stage": "A" | "B" | "C", "outcome": "SAVED" | "REJECTED" | "CANCELLED" | "TIMED_OUT", "reason": "string | null", "result": { "...": "stage-specific, e.g. Stage C's curve/table/max/residual" } }
```

`reason` is populated for `REJECTED`/`TIMED_OUT` with the same human-readable explanation the UI
guide's "if it fails" section prepares the user for (FR-008, FR-015, FR-026).

## `settings_changed`

```json
{ "type": "settings_changed", "schema": 1, "ssid": "NewNetworkName", "applies_in_s": 5 }
```

Sent to every connected client (including the one that made the change) so a second open browser
tab reflects a settings change made elsewhere (spec Edge Cases: two clients open at once).

## `error`

```json
{ "type": "error", "schema": 1, "code": "STRING_CODE", "message": "human-readable" }
```

Reserved for transport-level problems (e.g. a subscription the server can't satisfy). Request-level
errors (bad input, calibration busy) are REST-level errors per `contracts/rest-api.md`, not
WebSocket messages, since every command goes over REST.

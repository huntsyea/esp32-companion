# Xiaozhi Companion Wire Protocol

Shared contract between this firmware repo and the Hermes adapter plugin (repo: `hermes-esp32`).

Wire identity:

- Transport: WebSocket, path `/xiaozhi/v1`
- `PROTOCOL_VERSION = 1` (plugin constant: `xiaozhi_companion/protocol.py`; firmware guard lives in the companion board `config.json` `product_ui` section)
- Audio: Opus, 16 kHz mono, 60 ms frames
- Control: JSON messages, session generations guard against stale/cancelled sessions

Change rule: both repos must bump `PROTOCOL_VERSION` together for any incompatible wire change. This contract is pre-1.0 and intentionally narrow; extend only via new JSON message types or new audio session headers, never by mutating existing semantics in place.

Validation: `scripts/tests/test_companion_validation.py` plus the hardware runbook in `docs/companion-validation.md`.

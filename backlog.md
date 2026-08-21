# Companion Device — Backlog & Brain Dump

Personal idea/backlog scratchpad for the **Blobatar voice-companion** on the
Waveshare ESP32-S3-Touch-LCD-1.54. This is a living document — capture ideas
raw first in the **Brain Dump** section, then triage them into the themed
backlogs below when they're concrete enough to act on.

Product context (so ideas stay grounded):
- Push-to-talk, half-duplex voice over a secure LAN WebSocket to a Hermes
  runtime on a Mac mini (see `docs/companion-validation.md`).
- Blobatar conversation UI + a synchronized clock screen, LVGL on the 1.54"
  touch LCD (see `docs/companion-baseline.md`).
- Current release explicitly **out of scope**: wake word, full duplex, OTA.

Legend — Priority: **P0** must / **P1** should / **P2** nice-to-have.
Status: ✦ inbox · ▶ doing · ✓ done · ⊘ dropped. Effort: S / M / L / XL.

---

## Brain Dump (raw ideas — dump freely, triage later)

Drop unfiltered thoughts here as they come. One line per idea. No need to
format, group, or prioritize. Triage into the themed sections when it has a
shape. Check off with `[x]` when it's moved.

- [x] Volume buttons show a visual volume indicator when clicked (moved → Display & Companion UI)
- [x] Blobatar eyes track the source of sound using the dual mics for L/R direction (moved → Display & Companion UI)

---

## Voice & Conversation

| Priority | Status | Effort | Item |
| --- | --- | --- | --- |

## Display & Companion UI

| Priority | Status | Effort | Item |
| --- | --- | --- | --- |
| P2 | ✓ | S | **Volume indicator on button press** — brief visual overlay showing volume level when the volume buttons are clicked, then dismisses. Shipped as the full-screen volume overlay in the status-screen merge (`feat/blobatar-voice-companion`). |
| P2 | ✦ | M | **Blobatar eyes track sound direction** — dual mics, estimate left/right (and ideally angle) of the audio source and pan the avatar's gaze/eyes toward it. Depends on direction-of-arrival / beamforming on the 2-mic capture path (ESP-SR or manual level/phase comparison); start coarse L/C/R, refine from there. |

## Networking & Integration

| Priority | Status | Effort | Item |
| --- | --- | --- | --- |
| P1 | ✦ | M | **Device screen snapshot over the production session** — firmware already ships `self.screen.snapshot` (MCP tool: LVGL → JPEG → multipart POST to a given URL), but the `xiaozhi_companion` adapter's narrow wire contract doesn't carry MCP. Extend the adapter (and `PROTOCOL.md`, version-gated) to send the `tools/call` over the live secure session and receive the JPEG, so a developer/agent can capture the real screen on demand without reconfiguring the device. Panel readback is a dead end (MISO unwired). |

## Power, Device & Ops

| Priority | Status | Effort | Item |
| --- | --- | --- | --- |
| P0 | ✦ | S | **Approve device pairing on the mini** — the board (alain00) authenticates at the WS layer but is rejected by Hermes user authorization; its pairing codes expired unapproved. Wake the board, then `hermes pairing approve` on the mini while it retries (see `docs/dev-environment.md`). |

## Bugs / Known Gaps

- [ ] Adapter logs an unhandled `websockets.ConnectionClosedError` traceback (`xiaozhi_companion/network.py` receive loop) every time the device drops without a close frame (deep sleep / power cut). Harmless but noisy — catch `ConnectionClosed` around the receive loop.

---

## Conventions

- **Scope guard**: the companion is push-to-talk and half-duplex today. Mark
  items that deliberately expand scope (wake word, full duplex, OTA) clearly.
- When you start work, pull the item here into a branch off
  `feat/blobatar-voice-companion` (the single canonical branch since the
  worktree merge) and note the linked commit.
- Keep hardware-only findings in `docs/companion-validation.md`; keep open
  questions / root-cause research in `docs/research/`. The development
  environment itself is documented in `docs/dev-environment.md`.

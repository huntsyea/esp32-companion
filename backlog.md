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
| P2 | ✦ | S | **Volume indicator on button press** — brief visual overlay showing volume level when the volume buttons are clicked, then dismisses (reuse the existing backlight/brightness overlay pattern if one exists). |
| P2 | ✦ | M | **Blobatar eyes track sound direction** — dual mics, estimate left/right (and ideally angle) of the audio source and pan the avatar's gaze/eyes toward it. Depends on direction-of-arrival / beamforming on the 2-mic capture path (ESP-SR or manual level/phase comparison); start coarse L/C/R, refine from there. |

## Networking & Integration

| Priority | Idea | Effort | Item |
| --- | --- | --- | --- |

## Power, Device & Ops

| Priority | Idea | Effort | Item |
| --- | --- | --- | --- |

## Bugs / Known Gaps

- [ ] _known bug or gap_

---

## Conventions

- **Scope guard**: the companion is push-to-talk and half-duplex today. Mark
  items that deliberately expand scope (wake word, full duplex, OTA) clearly.
- When you start work, pull the item here into a branch (this tree uses
  `worktree/blobatar-voice-companion`) and note the linked commit.
- Keep hardware-only findings in `docs/companion-validation.md`; keep open
  questions / root-cause research in `research.md` / `context.md`.

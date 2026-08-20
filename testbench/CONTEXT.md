# ESP32 Companion Context

## Product

The product is a voice companion for the Waveshare ESP32-S3-Touch-LCD-1.54.
It presents a fixed animated Blobatar identity and a separate digital clock.
Voice requests are processed by Hermes Agent on a Mac mini on the same LAN.

## Current state

- The ESP-IDF board adapter initializes the ST7789 display, CST816 touch
  controller, backlight, and LVGL.
- The shared application UI runs on the device and in a native macOS SDL
  simulator.
- A full factory flash backup exists outside this project.
- The selected Blobatar identity and an exact SVG reference are pinned.
- Audio codecs, Wi-Fi, time synchronization, Xiaozhi transport, and Hermes
  integration are not implemented in this test-bench project yet.

## Target architecture

- The production device firmware is based on the maintained Xiaozhi board port
  so that microphone capture, Opus, speaker playback, and device state handling
  are reused.
- The shared LVGL product module owns the Avatar Screen, Clock Screen,
  navigation, and visible conversation state.
- A Hermes Platform Adapter accepts the device's authenticated Xiaozhi-compatible
  WebSocket session and maps voice messages to Hermes.
- The device renders all animation locally. Hermes sends audio, transcript data,
  and semantic events; it does not send pixels or video.

## Domain glossary

- **Companion Device**: the physical Waveshare ESP32-S3 unit.
- **Device Firmware**: the ESP-IDF/Xiaozhi program running on the Companion
  Device.
- **Product UI**: the platform-independent LVGL module shared by the simulator
  and Device Firmware.
- **Blobatar Identity**: the pinned `alain00` generation-2 capsule produced with
  shape trait `0.65` and hue `225`.
- **Avatar Screen**: the animated assistant surface.
- **Clock Screen**: the local digital clock surface.
- **Screen Manager**: the Product UI owner of screen selection and temporary
  voice-driven navigation.
- **Conversation State**: one of offline, idle, listening, thinking, speaking,
  or error.
- **Audio Session**: one push-to-talk request and its response audio.
- **Hermes Host**: the Mac mini running Hermes Agent and the Platform Adapter.
- **Platform Adapter**: the Hermes plugin that translates the device protocol
  into Hermes messages and responses.
- **Protocol Harness**: a desktop client that simulates one Companion Device
  session for acceptance testing.
- **Hardware Smoke Test**: a focused physical check of display, touch, audio,
  networking, memory, and recovery behavior.

## Invariants

- The Blobatar generation, identity inputs, resolved colors, and reference
  geometry must not drift during dependency upgrades.
- The Clock Screen must work without Hermes after time has synchronized.
- Network delays must not block LVGL animation or touch input.
- Conversation State transitions have one owner; transport code must not modify
  LVGL objects directly.
- Secrets and network-specific addresses are runtime configuration.
- Physical firmware changes require simulator or host-side validation first
  when the behavior is testable without hardware.
- The factory backup must not be modified.


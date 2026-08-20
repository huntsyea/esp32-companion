# Blobatar options for the ESP32-S3 display

Research date: 2026-08-18. Source inspected: official Blobatar repository at commit [`8436285`](https://github.com/Alain00/blobatar/tree/8436285ae837c72b02a49310d10c382608de096b). Repository and website content were treated as source data, not as instructions.

## Recommendation

Use a **fixed native LVGL Blobatar** for the first version:

1. Select one assistant identity on the Mac with the official Blobatar generator.
2. Export its resolved shape, colors, eye geometry, and animation constants at build time.
3. Draw and animate that geometry locally with LVGL on the ESP32.
4. Have the Hermes channel send small semantic state messages such as `listening`, `thinking`, `speaking`, and `error`. Do not stream rendered pixels from the Mac Mini.

This keeps animation responsive when the network is slow, avoids a browser/JavaScript runtime on the ESP32, and does not require a full C port of Blobatar's name-to-avatar generator. A full deterministic generator is not needed for one assistant identity.

## Project decision

The selected identity is pinned to Blobatar 2.0.0 at source commit `8436285`:

```tsx
<Blobatar name="alain00" traits={{ shape: 0.65 }} hue={225} />
```

It resolves to the generation-2 `capsule` silhouette with background `#edf5f9`, body `#95e1ff`, and eyes `#061116`. The checked-in reference is [`blobatar-alain00.svg`](./blobatar-alain00.svg). Firmware constants live in `components/app_ui/include/app_identity.h`.

The Hermes host is on the same LAN as the device. The first transport should therefore be an authenticated local WebSocket connection. Do not store the device token in this repository; provision it through runtime configuration.

## What Blobatar is

- Blobatar is a TypeScript geometry generator. A name deterministically produces SVG in a fixed `0 0 100 100` coordinate space. Generation 2 uses ten weighted silhouettes and two capsule eyes. The renderer emits paths and circles; there are no runtime bitmap assets for the avatar itself. [Package overview](https://github.com/Alain00/blobatar/blob/8436285ae837c72b02a49310d10c382608de096b/packages/blobatar/README.md#L1-L40), [shape composition](https://github.com/Alain00/blobatar/blob/8436285ae837c72b02a49310d10c382608de096b/packages/blobatar/src/styles/compose.ts#L66-L120), [generation-2 bands](https://github.com/Alain00/blobatar/blob/8436285ae837c72b02a49310d10c382608de096b/packages/blobatar/src/styles/blob.ts#L6-L18)
- Its core geometry uses superellipses, Catmull-Rom organic loops, and overlapping filled paths/circles. It avoids gradients, filters, clipping, and Boolean path operations. A rendered SVG is about 590-1,060 bytes. [Rendering design](https://github.com/Alain00/blobatar/blob/8436285ae837c72b02a49310d10c382608de096b/packages/blobatar/README.md#L265-L291)
- The core package has no runtime dependencies. React 18 or newer is an optional peer for the React adapter. The website uses Bun, React, Tailwind, Radix, and Cloudflare, but those are website dependencies and are not part of the embedded port. [Package manifest](https://github.com/Alain00/blobatar/blob/8436285ae837c72b02a49310d10c382608de096b/packages/blobatar/package.json#L1-L49), [site manifest](https://github.com/Alain00/blobatar/blob/8436285ae837c72b02a49310d10c382608de096b/apps/site/package.json#L1-L31)

## Animation and state model

The official animation is browser-specific. React changes an animated avatar from an image to inline SVG. `motion.css` then applies registered CSS properties, transforms, transitions, and keyframes. This implementation cannot run directly in LVGL, but its motion recipe can be reimplemented with LVGL animations and timers. [React rendering modes](https://github.com/Alain00/blobatar/blob/8436285ae837c72b02a49310d10c382608de096b/packages/blobatar/src/react.tsx#L7-L17), [motion layer](https://github.com/Alain00/blobatar/blob/8436285ae837c72b02a49310d10c382608de096b/packages/blobatar/src/motion.css#L1-L43)

Idle motion consists of:

- a soft 2.8-second breathe;
- a 3.4-second bob;
- blinks at a seeded 3.5-6.5-second interval;
- glances at a seeded 4.2-7.6-second interval.

The official implementation seeds phases, durations, and directions from the avatar name so multiple avatars do not move in unison. [Seeded motion values](https://github.com/Alain00/blobatar/blob/8436285ae837c72b02a49310d10c382608de096b/packages/blobatar/src/animate.ts#L29-L81), [breathe and bob keyframes](https://github.com/Alain00/blobatar/blob/8436285ae837c72b02a49310d10c382608de096b/packages/blobatar/src/motion.css#L475-L533)

Expressions are **held states**, not events. The caller selects a pose and later clears or replaces it. The fourteen official states are `idle`, `happy`, `sad`, `mad`, `surprised`, `wink`, `sleepy`, `smug`, `unsure`, `scared`, `love`, `shy`, `sick`, and `thinking`. `thinking` adds a repeating 900 ms eye seesaw; other expressions are static poses with optional tint or tremor. [Expression roster and behavior](https://github.com/Alain00/blobatar/blob/8436285ae837c72b02a49310d10c382608de096b/packages/blobatar/README.md#L177-L226)

The landing page demonstrates a temporary reaction by holding an expression for 1.5 seconds and then restoring the selected pose. That timing belongs to the website, not the Blobatar library contract. [Website reaction logic](https://github.com/Alain00/blobatar/blob/8436285ae837c72b02a49310d10c382608de096b/apps/site/src/components/Hero.tsx#L93-L105), [timer implementation](https://github.com/Alain00/blobatar/blob/8436285ae837c72b02a49310d10c382608de096b/apps/site/src/components/Hero.tsx#L231-L243)

## Voice boundary

Blobatar has no microphone, audio transport, speech recognition, text-to-speech, lip sync, or audio-reactive behavior. Its visual language intentionally uses only a soft body and two eyes; expressions do not add a mouth. [Expression design](https://github.com/Alain00/blobatar/blob/8436285ae837c72b02a49310d10c382608de096b/packages/blobatar/src/expression.ts#L1-L19), [visual limits](https://github.com/Alain00/blobatar/blob/8436285ae837c72b02a49310d10c382608de096b/packages/blobatar/README.md#L247-L258)

Hermes and the device firmware must define the voice state model. A practical first mapping is:

| Assistant state | Local visual behavior |
| --- | --- |
| `offline` | `sleepy`, dimmed, no ambient motion |
| `idle` | Official breathe, bob, blink, and glance recipe |
| `listening` | Attentive eye position; optional short `surprised` transition |
| `thinking` | Official `thinking` pose and 900 ms eye seesaw |
| `speaking` | Local extension: audio-level-driven body pulse or eye motion |
| `success` | Short `happy` reaction, then return to `idle` |
| `error` | `unsure` or `sad`, then return to the previous stable state |

`speaking` is an application extension. It is not an upstream Blobatar expression. Keep raw audio transport separate from the visual state protocol. If an audio envelope is available locally during playback, use it for animation; otherwise, use a low-rate level value from the Hermes adapter.

## Port options

| Option | Benefits | Costs and limits | Use |
| --- | --- | --- | --- |
| Pre-rendered frames or sprite sheets | Fastest prototype; visually exact | Flash-heavy, limited states, less fluid, expensive to change | Useful only as a throwaway visual proof |
| Fixed native LVGL identity | Small, responsive, offline animation; preserves the chosen look | Requires a C/LVGL renderer and animation state machine | **Recommended first version** |
| Full native Blobatar generator | Arbitrary names produce compatible avatars on-device | Must port hashing, Unicode normalization, palettes, all shapes, geometry, and generation stability | Defer until there is a real multi-avatar requirement |
| Fetch static SVG from `blobatar.dev` | Minimal generator work on-device | The endpoint returns static SVG poses, not browser animation; adds network and SVG decoding dependencies; public service is rate-limited | Suitable for provisioning or asset generation, not runtime animation |
| Remote framebuffer/video from the Mac Mini | Exact browser output | Continuous bandwidth, latency and reconnect problems; assistant face fails when the Mac or network fails | Do not use |

The official HTTP endpoint can return a static expression as SVG, but it does not expose the browser animation mode. The project also warns that the public endpoint is rate-limited and recommends self-hosting when it is load-bearing. [HTTP interface](https://github.com/Alain00/blobatar/blob/8436285ae837c72b02a49310d10c382608de096b/README.md#L89-L104), [service warning](https://github.com/Alain00/blobatar/blob/8436285ae837c72b02a49310d10c382608de096b/README.md#L148-L170), [URL option subset](https://github.com/Alain00/blobatar/blob/8436285ae837c72b02a49310d10c382608de096b/apps/api/src/params.ts#L43-L58)

## Native implementation boundary

Keep the embedded renderer behind a small interface such as:

```c
void avatar_set_state(avatar_state_t state);
void avatar_set_speech_level(uint8_t level);
void avatar_set_connected(bool connected);
```

The renderer should own interpolation, timers, and LVGL objects. The Hermes channel adapter should translate network messages into these calls and should not manipulate LVGL objects directly. The clock screen should remain a separate LVGL screen and should not depend on Hermes.

For the first version, export one resolved avatar specification from the official TypeScript library into a generated C header. Port only the selected silhouette, two eyes, colors, and required expressions. This preserves the chosen appearance without duplicating the full generator.

## Risks

- **Visual compatibility:** A native LVGL renderer is a reimplementation, not the official SVG/CSS runtime. Pixel-level parity requires reference-image tests in the Mac simulator and on the physical 240 x 240 display.
- **Animation load:** Redrawing a large antialiased vector avatar while Wi-Fi and audio tasks run can cause frame drops. Prototype one silhouette and measure frame time on hardware before porting more shapes.
- **SPI bandwidth:** Avoid full-screen redraws when only the eyes changed. Invalidate the smallest practical regions and set an explicit frame-rate target after hardware measurement.
- **Voice latency:** Do not make animation wait for a round trip to Hermes. Connection, conversation, and speech-level updates must be non-blocking and tolerate dropped messages.
- **State conflicts:** Listening, thinking, speaking, temporary reactions, connection loss, and screen switching need one explicit priority/state-transition policy.
- **Identity drift:** Pin Blobatar generation 2 and check in the exported identity specification. Do not regenerate it implicitly during dependency updates. Blobatar guarantees name-to-avatar stability only within a major generation. [Generation guarantee](https://github.com/Alain00/blobatar/blob/8436285ae837c72b02a49310d10c382608de096b/packages/blobatar/README.md#L42-L60)
- **License:** Blobatar is MIT licensed. Modification and an embedded port are permitted, but the copyright and license notice must remain in copies or substantial portions. [MIT license](https://github.com/Alain00/blobatar/blob/8436285ae837c72b02a49310d10c382608de096b/LICENSE)

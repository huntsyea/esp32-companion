# Blobatar reference frames

The `alain00-*.ppm` files are pinned 240 by 240 simulator renders for the
generation-2 Blobatar identity. The idle frame was checked against the resolved
SVG from Blobatar commit `8436285ae837c72b02a49310d10c382608de096b`.

Each conversation state uses a fixed animation time and playback level:

| State | Time (ms) | Playback envelope |
| --- | ---: | ---: |
| offline | 0 | 0 |
| idle | 0 | 0 |
| listening | 800 | 0 |
| thinking | 1200 | 0 |
| speaking | 400 | 800 |
| error | 0 | 0 |

Regenerate a frame only when an intentional visual change is approved:

```sh
./simulator/run.sh --render-reference simulator/reference/alain00-idle.ppm
./simulator/run.sh --render-state speaking 400 800 \
  simulator/reference/alain00-speaking.ppm
```

`scripts/tests/test_blobatar_reference.py` compares all rendered pixels with
an explicit antialiasing tolerance. It also checks the colors, geometry bounds,
content area, and unclipped display border. `scripts/tests/test_product_ui_state.py`
checks all state frames at their fixed inputs and samples bounded idle motion.

## Clock reference frames

`clock-noon.ppm` renders UTC epoch `1768500000` through the configured
`America/Chicago` POSIX rule. It verifies 12-hour noon formatting, the weekday,
and the date. `clock-unsynchronized.ppm` pins the explicit network-time failure
surface. Both frames use the same native Clock Screen source as the device.

```sh
./simulator/run.sh --render-clock 1768500000 simulator/reference/clock-noon.ppm
./simulator/run.sh --render-clock-unsynchronized \
  simulator/reference/clock-unsynchronized.ppm
```

`scripts/tests/test_companion_clock.py` also injects UTC epochs for midnight,
date rollover, both Chicago daylight-saving transitions, and continued clock
advancement without transport events.

## Screen navigation trace

The Screen Manager is the only module that changes Avatar and Clock Screen
visibility. A left swipe selects the Clock Screen. A right swipe returns to the
Avatar Screen. The interactive simulator also maps these actions to `.` and
`,` respectively.

Use the deterministic trace to inspect remembered selection, temporary voice
focus, the 1500 ms idle-return delay, and push-to-talk gesture blocking:

```sh
./simulator/run.sh --trace-navigation \
  left,connected,listen,recording_end,tts_start,tts_stop,time=1500
```

`scripts/tests/test_screen_manager.py` covers both directions, non-wrapping
screen edges, push-to-talk blocking, cancellation, renewed activity, error
recovery, and concurrent clock and Avatar animation updates.

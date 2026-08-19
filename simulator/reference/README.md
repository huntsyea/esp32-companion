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

# Blobatar reference frame

`alain00-idle.ppm` is the pinned 240 by 240 simulator render for the static
generation-2 Blobatar identity. It was checked against the resolved SVG from
Blobatar commit `8436285ae837c72b02a49310d10c382608de096b`.

Regenerate the frame only when an intentional visual change is approved:

```sh
./simulator/run.sh --render-reference simulator/reference/alain00-idle.ppm
```

`scripts/tests/test_blobatar_reference.py` compares all rendered pixels with
an explicit antialiasing tolerance. It also checks the colors, geometry bounds,
content area, and unclipped display border.

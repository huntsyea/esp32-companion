import math
import os
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
IDENTITY_HEADER = ROOT / "main" / "product_ui" / "product_identity.h"
PRODUCT_UI_SOURCE = ROOT / "main" / "product_ui" / "product_ui.c"
BLOBATAR_LICENSE = ROOT / "main" / "product_ui" / "BLOBATAR_LICENSE.txt"
REFERENCE_IMAGE = ROOT / "simulator" / "reference" / "alain00-idle.ppm"

DISPLAY_SIZE = (240, 240)
BACKGROUND = (0xED, 0xF5, 0xF9)
BODY = (0x95, 0xE1, 0xFF)
EYES = (0x06, 0x11, 0x16)

# SDL and the device use different color depths. Ignore small quantization and
# antialiasing differences, but reject a shifted edge or changed silhouette.
MAX_CHANNEL_DELTA = 12
MAX_OUTLIER_PIXELS = 288
MAX_RMSE = 3.0


def read_ppm(path):
    with path.open("rb") as image:
        self_identifying = image.readline().strip()
        dimensions = image.readline().strip()
        maximum = image.readline().strip()
        pixels = image.read()

    if self_identifying != b"P6" or maximum != b"255":
        raise AssertionError(f"Unsupported PPM format in {path}")

    width, height = (int(value) for value in dimensions.split())
    expected_bytes = width * height * 3
    if len(pixels) != expected_bytes:
        raise AssertionError(
            f"Expected {expected_bytes} pixel bytes in {path}, got {len(pixels)}"
        )
    return (width, height), pixels


def pixel_at(pixels, x, y):
    offset = (y * DISPLAY_SIZE[0] + x) * 3
    return tuple(pixels[offset : offset + 3])


def color_distance(left, right):
    return max(abs(a - b) for a, b in zip(left, right))


def color_bounds(pixels, color):
    points = []
    for y in range(DISPLAY_SIZE[1]):
        for x in range(DISPLAY_SIZE[0]):
            if color_distance(pixel_at(pixels, x, y), color) <= MAX_CHANNEL_DELTA:
                points.append((x, y))

    if not points:
        raise AssertionError(f"Color {color} is missing from the rendered image")
    return (
        min(x for x, _ in points),
        min(y for _, y in points),
        max(x for x, _ in points),
        max(y for _, y in points),
        len(points),
    )


class BlobatarReferenceTests(unittest.TestCase):
    def test_identity_contract_is_pinned(self):
        header = IDENTITY_HEADER.read_text(encoding="utf-8")

        self.assertIn('#define PRODUCT_IDENTITY_NAME "alain00"', header)
        self.assertIn("#define PRODUCT_IDENTITY_GENERATION 2", header)
        self.assertIn("#define PRODUCT_IDENTITY_SHAPE_TRAIT 0.65F", header)
        self.assertIn("#define PRODUCT_IDENTITY_HUE 225", header)
        self.assertIn("#define PRODUCT_IDENTITY_BACKGROUND_COLOR 0xEDF5F9", header)
        self.assertIn("#define PRODUCT_IDENTITY_BODY_COLOR 0x95E1FF", header)
        self.assertIn("#define PRODUCT_IDENTITY_EYE_COLOR 0x061116", header)
        self.assertIn("#define PRODUCT_IDENTITY_BODY_X 35", header)
        self.assertIn("#define PRODUCT_IDENTITY_BODY_Y 77", header)
        self.assertIn("#define PRODUCT_IDENTITY_BODY_WIDTH 165", header)
        self.assertIn("#define PRODUCT_IDENTITY_BODY_HEIGHT 92", header)
        self.assertIn("#define PRODUCT_IDENTITY_EYE_ROTATION 120", header)

        product_ui = PRODUCT_UI_SOURCE.read_text(encoding="utf-8")
        self.assertNotIn("http://", product_ui)
        self.assertNotIn("https://", product_ui)

        license_notice = BLOBATAR_LICENSE.read_text(encoding="utf-8")
        self.assertIn("MIT License", license_notice)
        self.assertIn("Copyright (c) 2026 Alain", license_notice)
        self.assertIn("8436285ae837c72b02a49310d10c382608de096b", license_notice)

    def test_simulator_render_matches_reference(self):
        self.assertTrue(REFERENCE_IMAGE.is_file(), "Missing pinned reference image")

        with tempfile.TemporaryDirectory() as temporary_directory:
            actual_image = Path(temporary_directory) / "alain00-idle.ppm"
            environment = os.environ.copy()
            environment.setdefault(
                "LVGL_SOURCE_DIR", str(ROOT / "managed_components" / "lvgl__lvgl")
            )
            completed = subprocess.run(
                [
                    str(ROOT / "simulator" / "run.sh"),
                    "--render-reference",
                    str(actual_image),
                ],
                cwd=ROOT,
                env=environment,
                capture_output=True,
                text=True,
                timeout=120,
                check=False,
            )

            self.assertEqual(
                completed.returncode,
                0,
                msg=f"{completed.stdout}\n{completed.stderr}",
            )
            self.assertTrue(actual_image.is_file())

            expected_size, expected_pixels = read_ppm(REFERENCE_IMAGE)
            actual_size, actual_pixels = read_ppm(actual_image)

        self.assertEqual(expected_size, DISPLAY_SIZE)
        self.assertEqual(actual_size, DISPLAY_SIZE)

        squared_error = 0
        outlier_pixels = 0
        for offset in range(0, len(expected_pixels), 3):
            expected = expected_pixels[offset : offset + 3]
            actual = actual_pixels[offset : offset + 3]
            deltas = [abs(a - b) for a, b in zip(expected, actual)]
            squared_error += sum(delta * delta for delta in deltas)
            if max(deltas) > MAX_CHANNEL_DELTA:
                outlier_pixels += 1

        rmse = math.sqrt(squared_error / len(expected_pixels))
        self.assertLessEqual(outlier_pixels, MAX_OUTLIER_PIXELS)
        self.assertLessEqual(rmse, MAX_RMSE)

        body_bounds = color_bounds(actual_pixels, BODY)
        eye_bounds = color_bounds(actual_pixels, EYES)
        self.assertEqual(body_bounds[:4], (35, 77, 199, 168))
        self.assertGreaterEqual(body_bounds[4], 11800)
        self.assertLessEqual(body_bounds[4], 12600)
        self.assertEqual(eye_bounds[:4], (88, 100, 156, 137))
        self.assertGreaterEqual(eye_bounds[4], 850)
        self.assertLessEqual(eye_bounds[4], 1050)

        for point, expected_color in (
            ((0, 0), BACKGROUND),
            ((120, 140), BODY),
            ((99, 119), EYES),
            ((146, 119), EYES),
        ):
            self.assertLessEqual(
                color_distance(pixel_at(actual_pixels, *point), expected_color),
                MAX_CHANNEL_DELTA,
                msg=f"Unexpected color at {point}",
            )

        border_points = [
            (x, y)
            for x in range(DISPLAY_SIZE[0])
            for y in range(DISPLAY_SIZE[1])
            if x < 12 or x >= 228 or y < 12 or y >= 228
        ]
        self.assertTrue(
            all(
                color_distance(pixel_at(actual_pixels, x, y), BACKGROUND)
                <= MAX_CHANNEL_DELTA
                for x, y in border_points
            ),
            "Avatar content reached the display edge",
        )


if __name__ == "__main__":
    unittest.main()

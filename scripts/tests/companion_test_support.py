import os
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def simulator_environment():
    environment = os.environ.copy()
    environment.setdefault(
        "LVGL_SOURCE_DIR", str(ROOT / "managed_components" / "lvgl__lvgl")
    )
    environment["SDL_VIDEODRIVER"] = "dummy"
    return environment


def ensure_simulator():
    completed = subprocess.run(
        [str(ROOT / "simulator" / "run.sh"), "--smoke-test"],
        cwd=ROOT,
        env=simulator_environment(),
        capture_output=True,
        text=True,
        timeout=120,
        check=False,
    )
    if completed.returncode != 0:
        raise AssertionError(f"{completed.stdout}\n{completed.stderr}")


def read_ppm(path):
    with path.open("rb") as image:
        identifier = image.readline().strip()
        dimensions = image.readline().strip()
        maximum = image.readline().strip()
        pixels = image.read()

    if identifier != b"P6" or dimensions != b"240 240" or maximum != b"255":
        raise AssertionError(f"Unexpected PPM header in {path}")
    if len(pixels) != 240 * 240 * 3:
        raise AssertionError(f"Unexpected PPM payload size in {path}")
    return pixels

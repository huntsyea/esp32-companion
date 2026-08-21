import calendar
import re
import subprocess
import tempfile
import unittest
from pathlib import Path

from scripts.tests.companion_test_support import (
    ensure_simulator,
    read_ppm,
    simulator_environment,
)


ROOT = Path(__file__).resolve().parents[2]
SIMULATOR = ROOT / "build" / "companion-simulator-macos" / "companion-simulator"
REFERENCE_DIRECTORY = ROOT / "simulator" / "reference"


def utc_epoch(year, month, day, hour, minute):
    return calendar.timegm((year, month, day, hour, minute, 0))


class CompanionClockTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        ensure_simulator()

    def run_simulator(self, *arguments):
        completed = subprocess.run(
            [str(SIMULATOR), *arguments],
            cwd=ROOT,
            env=simulator_environment(),
            capture_output=True,
            text=True,
            timeout=30,
            check=False,
        )
        self.assertEqual(
            completed.returncode,
            0,
            msg=f"{completed.stdout}\n{completed.stderr}",
        )
        return completed.stdout.strip()

    def inspect_epoch(self, epoch):
        output = self.run_simulator("--inspect-clock", str(epoch))
        # Values may contain spaces (e.g. date=Jan 15, 2026), so split on the
        # next `key=` boundary rather than on whitespace.
        return dict(re.findall(r"(\w+)=(.*?)(?=\s+\w+=|$)", output))

    def test_midnight_noon_and_date_rollover(self):
        midnight = self.inspect_epoch(utc_epoch(2026, 1, 15, 6, 0))
        noon = self.inspect_epoch(utc_epoch(2026, 1, 15, 18, 0))
        before_rollover = self.inspect_epoch(utc_epoch(2026, 1, 16, 5, 59))
        after_rollover = self.inspect_epoch(utc_epoch(2026, 1, 16, 6, 0))

        self.assertEqual(midnight["time"], "12:00")
        self.assertEqual(midnight["period"], "AM")
        self.assertEqual(midnight["weekday"], "Thu")
        self.assertEqual(midnight["date"], "Jan 15, 2026")
        self.assertEqual(noon["time"], "12:00")
        self.assertEqual(noon["period"], "PM")
        self.assertEqual(before_rollover["date"], "Jan 15, 2026")
        self.assertEqual(after_rollover["date"], "Jan 16, 2026")

    def test_chicago_daylight_saving_transitions(self):
        spring_before = self.inspect_epoch(utc_epoch(2026, 3, 8, 7, 59))
        spring_after = self.inspect_epoch(utc_epoch(2026, 3, 8, 8, 0))
        fall_before = self.inspect_epoch(utc_epoch(2026, 11, 1, 6, 59))
        fall_after = self.inspect_epoch(utc_epoch(2026, 11, 1, 7, 0))

        self.assertEqual((spring_before["time"], spring_before["dst"]), ("1:59", "0"))
        self.assertEqual((spring_after["time"], spring_after["dst"]), ("3:00", "1"))
        self.assertEqual((fall_before["time"], fall_before["dst"]), ("1:59", "1"))
        self.assertEqual((fall_after["time"], fall_after["dst"]), ("1:00", "0"))

    def test_clock_advances_from_system_epoch_without_transport_input(self):
        first = self.inspect_epoch(utc_epoch(2026, 8, 19, 17, 41))
        second = self.inspect_epoch(utc_epoch(2026, 8, 19, 17, 42))
        self.assertEqual(first["time"], "12:41")
        self.assertEqual(second["time"], "12:42")

    def test_same_minute_does_not_redraw_the_clock(self):
        start = utc_epoch(2026, 8, 19, 17, 41)
        output = self.run_simulator(
            "--trace-clock-updates", str(start), str(start + 30), str(start + 60)
        )
        self.assertEqual(
            output,
            "first=changed same_minute=unchanged next_minute=changed",
        )

    def test_unsynchronized_state_is_explicit(self):
        output = self.run_simulator("--inspect-clock-unsynchronized")
        self.assertEqual(output, "sync=no status=TIME_NOT_SYNCED")

    def test_runtime_timezone_accepts_chicago_and_falls_back_from_invalid_name(self):
        epoch = str(utc_epoch(2026, 3, 8, 8, 0))
        configured = self.run_simulator(
            "--inspect-clock-zone", "America/Chicago", epoch
        )
        fallback = self.run_simulator("--inspect-clock-zone", "Invalid/Zone", epoch)

        self.assertIn(
            "requested=America/Chicago active=America/Chicago fallback=no", configured
        )
        self.assertIn("time=3:00", configured)
        self.assertIn(
            "requested=Invalid/Zone active=America/Chicago fallback=yes", fallback
        )
        self.assertIn("time=3:00", fallback)

    def test_fixed_clock_snapshots_are_deterministic(self):
        cases = {
            "clock-noon": ("--render-clock", str(utc_epoch(2026, 1, 15, 18, 0))),
            "clock-unsynchronized": ("--render-clock-unsynchronized",),
        }
        with tempfile.TemporaryDirectory() as temporary_directory:
            directory = Path(temporary_directory)
            for name, arguments in cases.items():
                with self.subTest(name=name):
                    reference = REFERENCE_DIRECTORY / f"{name}.ppm"
                    self.assertTrue(reference.is_file(), f"Missing reference for {name}")
                    first = directory / f"{name}-first.ppm"
                    second = directory / f"{name}-second.ppm"
                    self.run_simulator(*arguments, str(first))
                    self.run_simulator(*arguments, str(second))
                    self.assertEqual(read_ppm(first), read_ppm(second))
                    self.assertEqual(read_ppm(first), read_ppm(reference))

    def test_clock_module_and_board_scoped_utc_sync_are_wired(self):
        clock_source = (ROOT / "main" / "product_ui" / "clock_screen.c").read_text(
            encoding="utf-8"
        )
        board_source = (
            ROOT
            / "main"
            / "boards"
            / "waveshare"
            / "esp32-s3-touch-lcd-1.54"
            / "esp32-s3-touch-lcd-1.54.cc"
        ).read_text(encoding="utf-8")
        ota_source = (ROOT / "main" / "ota.cc").read_text(encoding="utf-8")

        self.assertNotIn("America/Chicago", clock_source)
        self.assertNotIn("CST6CDT", clock_source)
        self.assertIn("companion_time_configure", board_source)
        self.assertIn('Settings clock_settings("clock")', board_source)
        self.assertIn('GetString("timezone", COMPANION_TIMEZONE_NAME)', board_source)
        self.assertIn("CONFIG_BOARD_TYPE_WAVESHARE_ESP32_S3_TOUCH_LCD_1_54", ota_source)


if __name__ == "__main__":
    unittest.main()

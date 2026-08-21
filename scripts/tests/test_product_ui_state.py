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
REFERENCE_CASES = {
    "offline": (0, 0),
    "idle": (0, 0),
    "listening": (800, 0),
    "thinking": (1200, 0),
    "speaking": (400, 800),
    "error": (0, 0),
}
BACKGROUND = (0xED, 0xF5, 0xF9)
EYES = (0x06, 0x11, 0x16)


def pixel_at(pixels, x, y):
    offset = (y * 240 + x) * 3
    return tuple(pixels[offset : offset + 3])


def color_points(pixels, color, tolerance=12):
    return [
        (x, y)
        for y in range(240)
        for x in range(240)
        if max(abs(left - right) for left, right in zip(pixel_at(pixels, x, y), color))
        <= tolerance
    ]


class ProductUiStateTests(unittest.TestCase):
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
        return completed.stdout.splitlines()

    def trace_states(self, events):
        lines = self.run_simulator("--trace-events", ",".join(events))
        return [
            field.removeprefix("state=")
            for line in lines
            for field in line.split()
            if field.startswith("state=")
        ]

    def test_valid_conversation_lifecycle(self):
        self.assertEqual(
            self.trace_states(
                ["connected", "listen", "recording_end", "tts_start", "tts_stop"]
            ),
            ["offline", "idle", "listening", "thinking", "speaking", "idle"],
        )

    def test_duplicate_events_are_idempotent(self):
        lines = self.run_simulator(
            "--trace-events",
            ",".join(
                [
                    "connected",
                    "connected",
                    "listen",
                    "listen",
                    "recording_end",
                    "recording_end",
                    "tts_start",
                    "tts_start",
                    "tts_stop",
                    "tts_stop",
                ]
            ),
        )
        ignored = [line for line in lines if "result=ignored" in line]
        self.assertEqual(len(ignored), 5)
        self.assertTrue(lines[-1].endswith("state=idle envelope=0"))

    def test_cancellation_returns_each_active_state_to_idle(self):
        for events in (
            ["connected", "listen", "cancel"],
            ["connected", "listen", "recording_end", "cancel"],
            ["connected", "listen", "recording_end", "tts_start", "cancel"],
        ):
            with self.subTest(events=events):
                self.assertEqual(self.trace_states(events)[-1], "idle")

    def test_temporary_error_recovers_prior_stable_state(self):
        self.assertEqual(
            self.trace_states(
                [
                    "connected",
                    "listen",
                    "recording_end",
                    "error",
                    "error",
                    "recover",
                    "tts_start",
                ]
            ),
            [
                "offline",
                "idle",
                "listening",
                "thinking",
                "error",
                "error",
                "thinking",
                "speaking",
            ],
        )

    def test_temporary_error_recovers_each_stable_state(self):
        stable_states = {
            "offline": [],
            "idle": ["connected"],
            "listening": ["connected", "listen"],
            "thinking": ["connected", "listen", "recording_end"],
            "speaking": ["connected", "listen", "recording_end", "tts_start"],
        }
        for state, prefix in stable_states.items():
            with self.subTest(state=state):
                states = self.trace_states(prefix + ["error", "recover"])
                self.assertEqual(states[-2:], ["error", state])

    def test_cancellation_clears_a_recoverable_connected_error(self):
        self.assertEqual(
            self.trace_states(["connected", "listen", "error", "cancel"])[-1],
            "idle",
        )

    def test_disconnect_and_reconnect_do_not_resume_stale_activity(self):
        for prefix in (
            ["connected", "listen"],
            ["connected", "listen", "recording_end"],
            ["connected", "listen", "recording_end", "tts_start"],
        ):
            with self.subTest(prefix=prefix):
                states = self.trace_states(prefix + ["disconnected", "connected", "listen"])
                self.assertEqual(states[-3:], ["offline", "idle", "listening"])

    def test_wifi_and_hermes_connection_statuses_are_independent(self):
        lines = self.run_simulator(
            "--trace-events",
            "wifi_connected,connected,disconnected,wifi_disconnected",
        )
        statuses = [
            (
                next(field for field in line.split() if field.startswith("wifi=")),
                next(field for field in line.split() if field.startswith("hermes=")),
            )
            for line in lines
        ]
        self.assertEqual(
            statuses,
            [
                ("wifi=disconnected", "hermes=disconnected"),
                ("wifi=connected", "hermes=disconnected"),
                ("wifi=connected", "hermes=connected"),
                ("wifi=connected", "hermes=disconnected"),
                ("wifi=disconnected", "hermes=disconnected"),
            ],
        )

    def test_speaking_envelope_is_smoothed_and_settles_on_silence(self):
        lines = self.run_simulator(
            "--trace-events",
            "connected,listen,recording_end,tts_start,level=800,time=100,"
            "time=250,level=0,time=450,time=650",
        )
        levels = [
            int(field.removeprefix("envelope="))
            for line in lines
            if line.startswith("action=time=")
            for field in line.split()
            if field.startswith("envelope=")
        ]
        self.assertEqual(levels, [400, 800, 400, 0])

    def test_fixed_time_state_references_are_deterministic(self):
        rendered_pixels = {}
        with tempfile.TemporaryDirectory() as temporary_directory:
            directory = Path(temporary_directory)
            for state, (time_ms, envelope) in REFERENCE_CASES.items():
                reference = REFERENCE_DIRECTORY / f"alain00-{state}.ppm"
                self.assertTrue(reference.is_file(), f"Missing reference for {state}")

                first = directory / f"{state}-first.ppm"
                second = directory / f"{state}-second.ppm"
                arguments = (
                    "--render-state",
                    state,
                    str(time_ms),
                    str(envelope),
                )
                self.run_simulator(*arguments, str(first))
                self.run_simulator(*arguments, str(second))

                expected_pixels = read_ppm(reference)
                first_pixels = read_ppm(first)
                second_pixels = read_ppm(second)
                self.assertEqual(first_pixels, second_pixels)
                self.assertEqual(first_pixels, expected_pixels)
                rendered_pixels[state] = first_pixels

        for left_index, left in enumerate(REFERENCE_CASES):
            for right in list(REFERENCE_CASES)[left_index + 1 :]:
                self.assertNotEqual(
                    rendered_pixels[left],
                    rendered_pixels[right],
                    f"{left} and {right} are not visually distinct",
                )

    def test_speaking_level_changes_visible_motion(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            directory = Path(temporary_directory)
            silent = directory / "silent.ppm"
            loud = directory / "loud.ppm"
            self.run_simulator("--render-state", "speaking", "300", "0", str(silent))
            self.run_simulator("--render-state", "speaking", "300", "900", str(loud))
            self.assertNotEqual(read_ppm(silent), read_ppm(loud))

    def test_idle_breathe_bob_glance_and_blink_remain_bounded(self):
        sample_times = (0, 1000, 3000, 4350)
        with tempfile.TemporaryDirectory() as temporary_directory:
            directory = Path(temporary_directory)
            frames = []
            for time_ms in sample_times:
                frame = directory / f"idle-{time_ms}.ppm"
                self.run_simulator("--render-state", "idle", str(time_ms), "0", str(frame))
                frames.append(read_ppm(frame))

        self.assertEqual(len(set(frames)), len(frames))

        base_eye_points = color_points(frames[0], EYES)
        glance_eye_points = color_points(frames[2], EYES)
        blink_eye_points = color_points(frames[3], EYES)
        self.assertGreater(min(x for x, _ in glance_eye_points), min(x for x, _ in base_eye_points))
        self.assertLess(len(blink_eye_points), len(base_eye_points))

        border_points = (
            [(x, y) for x in range(240) for y in range(12)]
            + [(x, y) for x in range(240) for y in range(228, 240)]
            + [(x, y) for x in range(12) for y in range(12, 228)]
            + [(x, y) for x in range(228, 240) for y in range(12, 228)]
        )
        for time_ms, frame in zip(sample_times, frames):
            self.assertTrue(
                all(pixel_at(frame, x, y) == BACKGROUND for x, y in border_points),
                f"Idle animation reached the display edge at {time_ms} ms",
            )


if __name__ == "__main__":
    unittest.main()

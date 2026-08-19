import subprocess
import unittest
from pathlib import Path

from scripts.tests.companion_test_support import ensure_simulator, simulator_environment


ROOT = Path(__file__).resolve().parents[2]
SIMULATOR = ROOT / "build" / "companion-simulator-macos" / "companion-simulator"


class ScreenManagerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        ensure_simulator()

    def trace(self, *actions):
        completed = subprocess.run(
            [str(SIMULATOR), "--trace-navigation", ",".join(actions)],
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
        return [
            dict(field.split("=", 1) for field in line.split())
            for line in completed.stdout.splitlines()
        ]

    def test_both_swipe_directions_and_edges_are_deterministic(self):
        trace = self.trace("right", "left", "left", "right", "right")

        self.assertEqual(
            [(line["result"], line["selected"], line["visible"]) for line in trace],
            [
                ("initial", "avatar", "avatar"),
                ("ignored", "avatar", "avatar"),
                ("changed", "clock", "clock"),
                ("ignored", "clock", "clock"),
                ("changed", "avatar", "avatar"),
                ("ignored", "avatar", "avatar"),
            ],
        )

    def test_push_to_talk_blocks_navigation_until_released(self):
        trace = self.trace("ptt=on", "left", "ptt=off", "left")

        self.assertEqual(trace[2]["result"], "ignored")
        self.assertEqual(trace[2]["selected"], "avatar")
        self.assertEqual(trace[-1]["result"], "changed")
        self.assertEqual(trace[-1]["selected"], "clock")

    def test_voice_activity_focuses_avatar_then_restores_clock(self):
        trace = self.trace(
            "left",
            "connected",
            "listen",
            "recording_end",
            "tts_start",
            "tts_stop",
            "time=1499",
            "time=1500",
        )

        self.assertEqual(trace[1]["visible"], "clock")
        self.assertEqual(trace[2]["visible"], "clock")
        for line in trace[3:8]:
            self.assertEqual(line["selected"], "clock")
            self.assertEqual(line["visible"], "avatar")
        self.assertEqual(trace[6]["return"], "pending")
        self.assertEqual(trace[7]["return"], "pending")
        self.assertEqual(trace[8]["return"], "none")
        self.assertEqual(trace[8]["visible"], "clock")

    def test_new_activity_cancels_a_pending_return(self):
        trace = self.trace(
            "left",
            "connected",
            "listen",
            "cancel",
            "time=800",
            "listen",
            "time=2300",
            "cancel",
            "time=3799",
            "time=3800",
        )

        self.assertEqual(trace[4]["return"], "pending")
        self.assertEqual(trace[5]["return"], "pending")
        self.assertEqual(trace[6]["return"], "none")
        self.assertEqual(trace[7]["visible"], "avatar")
        self.assertEqual(trace[8]["return"], "pending")
        self.assertEqual(trace[9]["visible"], "avatar")
        self.assertEqual(trace[10]["visible"], "clock")

    def test_error_focus_is_temporary(self):
        trace = self.trace(
            "left", "connected", "error", "recover", "time=1499", "time=1500"
        )

        self.assertEqual(trace[3]["state"], "error")
        self.assertEqual(trace[3]["visible"], "avatar")
        self.assertEqual(trace[4]["state"], "idle")
        self.assertEqual(trace[4]["return"], "pending")
        self.assertEqual(trace[-1]["visible"], "clock")

    def test_clock_touch_and_avatar_animation_updates_do_not_block_each_other(self):
        trace = self.trace(
            "left",
            "clock=1787161200",
            "animation=4350",
            "clock=1787161260",
            "right",
        )

        self.assertEqual(trace[2]["clock"], "changed")
        self.assertEqual(trace[3]["animation"], "applied")
        self.assertEqual(trace[4]["clock"], "changed")
        self.assertEqual(trace[4]["visible"], "clock")
        self.assertEqual(trace[5]["visible"], "avatar")

    def test_device_and_simulator_wire_native_gestures_to_the_manager(self):
        board_source = (
            ROOT
            / "main"
            / "boards"
            / "waveshare"
            / "esp32-s3-touch-lcd-1.54"
            / "esp32-s3-touch-lcd-1.54.cc"
        ).read_text(encoding="utf-8")
        simulator_source = (ROOT / "simulator" / "main.c").read_text(encoding="utf-8")
        cmake_source = (ROOT / "main" / "CMakeLists.txt").read_text(encoding="utf-8")

        for source in (board_source, simulator_source):
            self.assertIn("LV_EVENT_GESTURE", source)
            self.assertIn("lv_indev_get_gesture_dir", source)
            self.assertIn("screen_manager_handle_swipe", source)
        self.assertIn('"product_ui/screen_manager.c"', cmake_source)


if __name__ == "__main__":
    unittest.main()

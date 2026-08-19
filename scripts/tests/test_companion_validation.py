import hashlib
import json
import subprocess
import tempfile
import unittest
from datetime import datetime, timezone
from pathlib import Path
from unittest import mock

from scripts.companion_validation import (
    ValidationHarness,
    render_report,
)


ROOT = Path(__file__).resolve().parents[2]


def write_baseline(root: Path, backup_payload: bytes = b"factory") -> Path:
    backup = root.parent / "backup" / "flash-full-16mb-20260818.bin"
    backup.parent.mkdir(parents=True)
    backup.write_bytes(backup_payload)
    (root / "companion").mkdir(parents=True)
    (root / "companion" / "production-baseline.json").write_text(
        json.dumps(
            {
                "schema_version": 1,
                "board": {
                    "directory": "waveshare/esp32-s3-touch-lcd-1.54",
                    "variant": "esp32-s3-touch-lcd-1.54",
                    "target": "esp32s3",
                },
                "factory_backup": {
                    "filename": backup.name,
                    "size_bytes": len(backup_payload),
                    "sha256": hashlib.sha256(backup_payload).hexdigest(),
                },
            }
        ),
        encoding="utf-8",
    )
    return backup


class CompanionValidationTests(unittest.TestCase):
    def test_factory_backup_verification_is_exact_and_read_only(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "repo"
            root.mkdir()
            backup = write_baseline(root)
            before = backup.stat()

            result = ValidationHarness(root).check_factory_backup()

            self.assertEqual("PASS", result.status)
            self.assertIn("SHA-256 verified", result.detail)
            self.assertEqual(before.st_size, backup.stat().st_size)
            self.assertEqual(before.st_mtime_ns, backup.stat().st_mtime_ns)

            backup.write_bytes(b"changed")
            mismatch = ValidationHarness(root).check_factory_backup()
            self.assertEqual("FAIL", mismatch.status)
            self.assertEqual("factory backup size or SHA-256 does not match baseline", mismatch.detail)

    def test_firmware_artifact_requires_matching_target_board_and_flash_layout(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "repo"
            root.mkdir()
            write_baseline(root)
            build = root / "build"
            build.mkdir()
            (build / "xiaozhi.bin").write_bytes(b"firmware")
            (build / "project_description.json").write_text(
                json.dumps(
                    {
                        "project_name": "xiaozhi",
                        "app_bin": "xiaozhi.bin",
                        "target": "esp32s3",
                        "git_revision": "v5.5.2",
                    }
                ),
                encoding="utf-8",
            )
            (build / "xiaozhi-build.sdkconfig.defaults").write_text(
                "CONFIG_BOARD_TYPE_WAVESHARE_ESP32_S3_TOUCH_LCD_1_54=y\n",
                encoding="utf-8",
            )
            (build / "flash_args").write_text(
                "--flash_size 16MB\n0x20000 xiaozhi.bin\n", encoding="utf-8"
            )

            result = ValidationHarness(root).check_firmware_artifact()

            self.assertEqual("PASS", result.status)
            self.assertIn("target=esp32s3", result.detail)
            self.assertIn("board=esp32-s3-touch-lcd-1.54", result.detail)
            self.assertIn("sha256=", result.detail)

            (build / "project_description.json").write_text(
                json.dumps(
                    {
                        "project_name": "xiaozhi",
                        "app_bin": "xiaozhi.bin",
                        "target": "esp32",
                    }
                ),
                encoding="utf-8",
            )
            mismatch = ValidationHarness(root).check_firmware_artifact()
            self.assertEqual("FAIL", mismatch.status)
            self.assertEqual("firmware target does not match baseline", mismatch.detail)

    @mock.patch("scripts.companion_validation.subprocess.run")
    def test_safe_gates_use_argv_without_a_shell_and_suppress_command_output(self, run):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "repo"
            root.mkdir()
            write_baseline(root)
            run.return_value = subprocess.CompletedProcess([], 0)
            harness = ValidationHarness(root)

            host = harness.run_host_tests()
            simulator = harness.run_simulator_smoke()
            firmware = harness.run_firmware_build()

            self.assertEqual(["PASS", "PASS", "PASS"], [host.status, simulator.status, firmware.status])
            commands = [call.args[0] for call in run.call_args_list]
            self.assertEqual(
                [
                    [harness.python, "-m", "unittest", "discover", "-s", "scripts/tests", "-v"],
                    [str(harness.root / "simulator" / "run.sh"), "--smoke-test"],
                    [
                        harness.python,
                        "scripts/build.py",
                        "waveshare/esp32-s3-touch-lcd-1.54",
                        "--name",
                        "esp32-s3-touch-lcd-1.54",
                    ],
                ],
                commands,
            )
            for call in run.call_args_list:
                self.assertFalse(call.kwargs.get("shell", False))
                self.assertEqual(subprocess.DEVNULL, call.kwargs["stdout"])
                self.assertEqual(subprocess.DEVNULL, call.kwargs["stderr"])

    def test_report_keeps_automated_evidence_separate_from_pending_hardware(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "repo"
            root.mkdir()
            write_baseline(root)
            harness = ValidationHarness(root)
            report = render_report(
                [
                    harness.not_run("host-tests", "Host test suite"),
                    harness.not_run("simulator", "Strict simulator smoke test"),
                    harness.not_run("firmware-build", "Canonical firmware build"),
                ],
                generated_at=datetime(2026, 8, 19, 12, 0, tzinfo=timezone.utc),
            )

            self.assertIn("## Automated evidence", report)
            self.assertIn("## Manual and physical acceptance", report)
            self.assertIn("| Host test suite | NOT RUN |", report)
            for expected in (
                "display color and orientation",
                "avatar identity and state animation",
                "touch and screen navigation",
                "touch response timing",
                "buttons and remembered-screen restoration",
                "microphone capture",
                "speaker playback",
                "Wi-Fi connection",
                "secure WSS connection",
                "mDNS discovery",
                "certificate trust",
                "clock synchronization",
                "live push-to-talk and TTS",
                "cancellation during recording and playback",
                "Mac mini service and device restarts",
                "free internal heap",
                "free PSRAM",
                "minimum task stack margins",
                "display and animation timing",
                "audio underruns",
                "end-to-end latency",
                "repeated-exchange memory stability",
            ):
                self.assertIn(f"| {expected} | PENDING |", report)
            self.assertIn("wake word, full duplex, and ota are out of scope", report.lower())
            self.assertNotIn("token=", report.lower())

    def test_runbook_and_cli_do_not_perform_destructive_or_external_setup(self):
        script = (ROOT / "scripts" / "companion_validation.py").read_text(encoding="utf-8")
        documentation = (ROOT / "docs" / "companion-validation.md").read_text(encoding="utf-8")

        for forbidden in ("write_flash", "erase_flash", "esptool", "hermes install", "bind("):
            self.assertNotIn(forbidden, script)
        self.assertIn("PENDING", documentation)
        self.assertIn("explicit approval", documentation)
        self.assertIn("shasum -a 256", documentation)
        self.assertIn("write_flash 0x0", documentation)
        self.assertIn("read_flash 0x0 0x1000000", documentation)
        self.assertIn("Do not put credentials", documentation)


if __name__ == "__main__":
    unittest.main()

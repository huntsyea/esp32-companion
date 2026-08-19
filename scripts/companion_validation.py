#!/usr/bin/env python3
"""Collect non-destructive Companion Device validation evidence."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import subprocess
import sys
import time
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Sequence


ROOT = Path(__file__).resolve().parents[1]
BASELINE_PATH = Path("companion/production-baseline.json")
DEFAULT_REPORT_PATH = Path("build/companion-validation-report.md")
BOARD_CONFIG_PREFIX = "CONFIG_BOARD_TYPE_"


@dataclass(frozen=True)
class ValidationResult:
    key: str
    label: str
    status: str
    detail: str


MANUAL_ACCEPTANCE = (
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
)


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _safe_detail(value: str) -> str:
    return " ".join(value.replace("|", "/").split())


def _board_config_symbol(directory: str, variant: str) -> str:
    vendor = Path(directory).parts[0]
    identifier = re.sub(r"[^A-Za-z0-9]+", "_", f"{vendor}_{variant}")
    return f"{BOARD_CONFIG_PREFIX}{identifier.upper().strip('_')}=y"


class ValidationHarness:
    def __init__(self, root: Path = ROOT) -> None:
        self.root = root.resolve()
        self.python = sys.executable
        self.baseline = json.loads(
            (self.root / BASELINE_PATH).read_text(encoding="utf-8")
        )

    @staticmethod
    def not_run(key: str, label: str) -> ValidationResult:
        return ValidationResult(key, label, "NOT RUN", "enable the corresponding safe gate")

    def check_repository(self) -> ValidationResult:
        try:
            top_level = subprocess.run(
                ["git", "rev-parse", "--show-toplevel"],
                cwd=self.root,
                check=False,
                stdout=subprocess.PIPE,
                stderr=subprocess.DEVNULL,
                text=True,
            )
            revision = subprocess.run(
                ["git", "rev-parse", "HEAD"],
                cwd=self.root,
                check=False,
                stdout=subprocess.PIPE,
                stderr=subprocess.DEVNULL,
                text=True,
            )
            status = subprocess.run(
                ["git", "status", "--porcelain"],
                cwd=self.root,
                check=False,
                stdout=subprocess.PIPE,
                stderr=subprocess.DEVNULL,
                text=True,
            )
        except OSError:
            return ValidationResult(
                "repository", "Repository state", "FAIL", "git could not be executed"
            )

        if any(command.returncode for command in (top_level, revision, status)):
            return ValidationResult(
                "repository", "Repository state", "FAIL", "git repository check failed"
            )
        if Path(top_level.stdout.strip()).resolve() != self.root:
            return ValidationResult(
                "repository", "Repository state", "FAIL", "script is not running at repository root"
            )

        changed_count = len(status.stdout.splitlines())
        current_revision = revision.stdout.strip()
        if changed_count:
            return ValidationResult(
                "repository",
                "Repository state",
                "FAIL",
                f"revision={current_revision}; dirty entries={changed_count}",
            )
        return ValidationResult(
            "repository", "Repository state", "PASS", f"revision={current_revision}; clean"
        )

    def check_factory_backup(self) -> ValidationResult:
        expected = self.baseline["factory_backup"]
        backup_path = self.root.parent / "backup" / expected["filename"]
        if backup_path.is_symlink() or not backup_path.is_file():
            return ValidationResult(
                "factory-backup", "Factory backup", "FAIL", "factory backup is missing"
            )

        size_matches = backup_path.stat().st_size == int(expected["size_bytes"])
        digest_matches = _sha256(backup_path) == expected["sha256"]
        if not size_matches or not digest_matches:
            return ValidationResult(
                "factory-backup",
                "Factory backup",
                "FAIL",
                "factory backup size or SHA-256 does not match baseline",
            )
        return ValidationResult(
            "factory-backup",
            "Factory backup",
            "PASS",
            (
                f"size={expected['size_bytes']} bytes; SHA-256 verified; "
                f"sha256={expected['sha256']}"
            ),
        )

    def check_firmware_artifact(self) -> ValidationResult:
        build = self.root / "build"
        description_path = build / "project_description.json"
        flash_args_path = build / "flash_args"
        board_defaults_path = build / "xiaozhi-build.sdkconfig.defaults"
        if not all(
            path.is_file()
            for path in (description_path, flash_args_path, board_defaults_path)
        ):
            return ValidationResult(
                "firmware-artifact",
                "Firmware artifact metadata",
                "FAIL",
                "canonical firmware metadata is incomplete",
            )

        try:
            description = json.loads(description_path.read_text(encoding="utf-8"))
        except (json.JSONDecodeError, OSError):
            return ValidationResult(
                "firmware-artifact",
                "Firmware artifact metadata",
                "FAIL",
                "project description is invalid",
            )

        board = self.baseline["board"]
        if description.get("target") != board["target"]:
            return ValidationResult(
                "firmware-artifact",
                "Firmware artifact metadata",
                "FAIL",
                "firmware target does not match baseline",
            )
        idf_revision = description.get("git_revision")
        if not isinstance(idf_revision, str) or not idf_revision:
            return ValidationResult(
                "firmware-artifact",
                "Firmware artifact metadata",
                "FAIL",
                "project description has no ESP-IDF revision",
            )
        app_name = description.get("app_bin")
        if not isinstance(app_name, str) or Path(app_name).name != app_name:
            return ValidationResult(
                "firmware-artifact",
                "Firmware artifact metadata",
                "FAIL",
                "project description has an invalid application artifact",
            )
        app_path = build / app_name
        if not app_path.is_file() or app_path.stat().st_size == 0:
            return ValidationResult(
                "firmware-artifact",
                "Firmware artifact metadata",
                "FAIL",
                "application artifact is missing or empty",
            )

        expected_board_symbol = _board_config_symbol(board["directory"], board["variant"])
        board_defaults = board_defaults_path.read_text(encoding="utf-8")
        if expected_board_symbol not in board_defaults.splitlines():
            return ValidationResult(
                "firmware-artifact",
                "Firmware artifact metadata",
                "FAIL",
                "firmware board does not match baseline",
            )
        flash_args = flash_args_path.read_text(encoding="utf-8")
        flash_lines = flash_args.splitlines()
        if "--flash_size 16MB" not in flash_lines[0] or f"0x20000 {app_name}" not in flash_lines:
            return ValidationResult(
                "firmware-artifact",
                "Firmware artifact metadata",
                "FAIL",
                "application artifact is absent from canonical flash layout",
            )

        digest = _sha256(app_path)
        return ValidationResult(
            "firmware-artifact",
            "Firmware artifact metadata",
            "PASS",
            (
                f"target={board['target']}; board={board['variant']}; idf={idf_revision}; "
                f"size={app_path.stat().st_size} bytes; sha256={digest}"
            ),
        )

    def _run_gate(
        self, key: str, label: str, command: Sequence[str], timeout: int
    ) -> ValidationResult:
        started = time.monotonic()
        try:
            completed = subprocess.run(
                list(command),
                cwd=self.root,
                check=False,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                shell=False,
                timeout=timeout,
                env=os.environ.copy(),
            )
        except subprocess.TimeoutExpired:
            return ValidationResult(key, label, "FAIL", f"timed out after {timeout} seconds")
        except OSError:
            return ValidationResult(key, label, "FAIL", "command could not be started")

        duration = time.monotonic() - started
        if completed.returncode:
            return ValidationResult(
                key, label, "FAIL", f"exit={completed.returncode}; output suppressed"
            )
        return ValidationResult(key, label, "PASS", f"completed in {duration:.1f} seconds")

    def run_host_tests(self) -> ValidationResult:
        return self._run_gate(
            "host-tests",
            "Host test suite",
            [self.python, "-m", "unittest", "discover", "-s", "scripts/tests", "-v"],
            timeout=1200,
        )

    def run_simulator_smoke(self) -> ValidationResult:
        return self._run_gate(
            "simulator",
            "Strict simulator smoke test",
            [str(self.root / "simulator" / "run.sh"), "--smoke-test"],
            timeout=300,
        )

    def run_firmware_build(self) -> ValidationResult:
        board = self.baseline["board"]
        return self._run_gate(
            "firmware-build",
            "Canonical firmware build",
            [
                self.python,
                "scripts/build.py",
                board["directory"],
                "--name",
                board["variant"],
            ],
            timeout=2400,
        )

    def write_report(self, report: str, results: Sequence[ValidationResult]) -> Path:
        report_path = (self.root / report).resolve()
        report_root = (self.root / "build").resolve()
        if report_root != report_path.parent and report_root not in report_path.parents:
            raise ValueError("report path must be inside the repository build directory")
        if report_path.suffix.lower() != ".md":
            raise ValueError("report path must use the .md extension")
        report_path.parent.mkdir(parents=True, exist_ok=True)
        report_path.write_text(render_report(results), encoding="utf-8")
        return report_path


def render_report(
    results: Sequence[ValidationResult], generated_at: datetime | None = None
) -> str:
    timestamp = generated_at or datetime.now(timezone.utc)
    lines = [
        "# Companion Device validation report",
        "",
        f"Generated: {timestamp.astimezone(timezone.utc).isoformat(timespec='seconds')}",
        "",
        "This report separates automated evidence from physical acceptance. A PASS build is not hardware validation.",
        "",
        "## Automated evidence",
        "",
        "| Gate | Status | Evidence |",
        "| --- | --- | --- |",
    ]
    for result in results:
        lines.append(
            f"| {_safe_detail(result.label)} | {result.status} | {_safe_detail(result.detail)} |"
        )

    lines.extend(
        [
            "",
            "## Manual and physical acceptance",
            "",
            "These checks remain PENDING until a person records physical evidence after an approved flash.",
            "",
            "| Check | Status | Evidence |",
            "| --- | --- | --- |",
        ]
    )
    for label in MANUAL_ACCEPTANCE:
        lines.append(f"| {label} | PENDING | not recorded |")

    lines.extend(
        [
            "",
            "## Scope and privacy",
            "",
            "Wake word, full duplex, and OTA are out of scope. Diagnostic evidence must not contain credentials, private keys, raw audio, or transcript content.",
            "",
        ]
    )
    return "\n".join(lines)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Collect non-destructive Companion Device validation evidence."
    )
    parser.add_argument("--run-host-tests", action="store_true")
    parser.add_argument("--run-simulator", action="store_true")
    parser.add_argument("--run-firmware-build", action="store_true")
    parser.add_argument(
        "--all-safe",
        action="store_true",
        help="run host tests, simulator smoke test, and canonical firmware build",
    )
    parser.add_argument(
        "--report",
        default=str(DEFAULT_REPORT_PATH),
        help="Markdown report path under build/",
    )
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    harness = ValidationHarness()
    run_host = args.all_safe or args.run_host_tests
    run_simulator = args.all_safe or args.run_simulator
    run_firmware = args.all_safe or args.run_firmware_build

    results = [
        harness.check_repository(),
        harness.check_factory_backup(),
        harness.run_host_tests()
        if run_host
        else harness.not_run("host-tests", "Host test suite"),
        harness.run_simulator_smoke()
        if run_simulator
        else harness.not_run("simulator", "Strict simulator smoke test"),
        harness.run_firmware_build()
        if run_firmware
        else harness.not_run("firmware-build", "Canonical firmware build"),
        harness.check_firmware_artifact(),
    ]

    try:
        report_path = harness.write_report(args.report, results)
    except (OSError, ValueError) as error:
        print(f"FAIL report: {_safe_detail(str(error))}", file=sys.stderr)
        return 2

    for result in results:
        print(f"{result.status:7} {result.label}: {result.detail}")
    print(f"REPORT  {report_path.relative_to(harness.root)}")
    return 1 if any(result.status == "FAIL" for result in results) else 0


if __name__ == "__main__":
    raise SystemExit(main())

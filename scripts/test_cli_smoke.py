#!/usr/bin/env python3
"""CLI smoke checks for wxvoice.

Runs the built executable against an empty data root and asserts the documented
exit codes, so argument handling and the "no assets" path stay stable without
touching real WeChat data.
"""

import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


EXE = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else None


def run(*args: str) -> subprocess.CompletedProcess:
    assert EXE is not None, "usage: test_cli_smoke.py <wxvoice executable>"
    return subprocess.run(
        [str(EXE), *args],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        timeout=60,
        check=False,
    )


def text(stream: bytes) -> str:
    return stream.decode("utf-8", errors="replace")


class TestCliSmoke(unittest.TestCase):
    def setUp(self) -> None:
        self.tmp = Path(tempfile.mkdtemp(prefix="wxvoice-cli-"))
        self.addCleanup(shutil.rmtree, self.tmp, ignore_errors=True)
        self.empty_root = self.tmp / "empty"
        self.empty_root.mkdir()

    def test_help(self) -> None:
        result = run("--help")
        self.assertEqual(0, result.returncode)
        self.assertIn("wxvoice", text(result.stdout))
        self.assertIn("--root", text(result.stdout))
        self.assertIn("--watch", text(result.stdout))

    def test_version(self) -> None:
        result = run("--version")
        self.assertEqual(0, result.returncode)
        self.assertRegex(text(result.stdout), r"wxvoice \d+\.\d+\.\d+")

    def test_list_empty_root_exits_three(self) -> None:
        result = run("list", "--root", str(self.empty_root))
        self.assertEqual(3, result.returncode)
        self.assertIn("未找到收藏语音资源", text(result.stderr))

    def test_list_missing_root_exits_two(self) -> None:
        result = run("list", "--root", str(self.tmp / "missing"))
        self.assertEqual(2, result.returncode)
        self.assertIn("不存在", text(result.stderr))

    def test_export_on_empty_root_exits_three(self) -> None:
        result = run("export", "1", "--root", str(self.empty_root))
        self.assertEqual(3, result.returncode)

    def test_unknown_command_exits_two(self) -> None:
        result = run("frobnicate")
        self.assertEqual(2, result.returncode)
        self.assertIn("未知命令", text(result.stderr))

    def test_missing_option_value_exits_two(self) -> None:
        result = run("list", "--root")
        self.assertEqual(2, result.returncode)
        self.assertIn("需要一个值", text(result.stderr))

    def test_export_requires_selector(self) -> None:
        result = run("export", "--root", str(self.empty_root))
        self.assertEqual(2, result.returncode)

    def test_export_watch_requires_value(self) -> None:
        result = run("export", "1", "--watch")
        self.assertEqual(2, result.returncode)
        self.assertIn("需要一个值", text(result.stderr))

    def test_export_watch_rejects_invalid_value(self) -> None:
        result = run("export", "1", "--root", str(self.empty_root), "--watch", "0")
        self.assertEqual(2, result.returncode)
        self.assertIn("--watch", text(result.stderr))

    def test_export_watch_stops_on_missing_asset(self) -> None:
        result = run("export", "1", "--root", str(self.empty_root), "--watch", "30")
        self.assertEqual(3, result.returncode)


def main() -> int:
    if EXE is None:
        print("usage: test_cli_smoke.py <wxvoice executable>", file=sys.stderr)
        return 2
    program = unittest.main(argv=[sys.argv[0]], exit=False)
    return 0 if program.result.wasSuccessful() else 1


if __name__ == "__main__":
    sys.exit(main())

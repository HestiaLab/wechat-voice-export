#!/usr/bin/env python3
"""Coverage ratchet: line coverage may only go up, never down.

Reads the gcov data produced by the project's coverage preset build
(``ninja-coverage`` by default: configure with coverage flags, build, run
CTest), computes the line-coverage percentage of the project's own sources
(``src/``), and compares it against a stored baseline.

Baseline policy: the baseline only moves upward. ``--update`` writes a new
baseline only when the measured coverage is greater than or equal to the
stored one; a measurement below the baseline fails the check.

This script is an OPTIONAL entry point: it is deliberately not wired into CI
or ``scripts/check.sh``. Coverage numbers are toolchain- and platform-sensitive,
so any threshold baseline must name a single platform of record.

Usage
-----
    # full cycle (recommended):
    cmake --preset ninja-coverage
    cmake --build --preset ninja-coverage
    python3 scripts/coverage_ratchet.py --reset-gcda <build-dir>   # clear stale counters
    ctest --preset ninja-coverage --output-on-failure             # regenerate counters
    python3 scripts/coverage_ratchet.py <build-dir>               # compare vs baseline
    python3 scripts/coverage_ratchet.py <build-dir> --update      # ratchet baseline up

Requires: GCC/Clang coverage build (``--coverage``) and the ``gcov`` tool on
PATH. Nothing project-specific is hard-coded: sources are discovered from the
gcov data itself and filtered to the repository's ``src/`` tree.

Exit codes: 0 = at or above baseline (or baseline written with --update);
1 = below baseline or measurement failed.
"""

import argparse
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


def _pin_utf8_console() -> None:
    """Pin stdout/stderr to UTF-8 so Chinese verdicts stay readable.

    A legacy console code page (Windows GBK/cp1252) either raises
    UnicodeEncodeError on stdout or degrades stderr to literal escape
    sequences, which turns a verdict into noise. The wizard CLI pins the
    console the same way (``SetConsoleOutputCP(CP_UTF8)``).
    """
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="replace")


_pin_utf8_console()


_BASELINE_FILENAME = "coverage_baseline.txt"
_GCOV_LINE_RE = re.compile(r"^\s*(#####|-|\d+[*!]?):")


def _fail(message: str) -> None:
    print(f"FAIL: {message}", file=sys.stderr)


def find_gcda_files(build_dir: Path) -> list[Path]:
    return sorted(build_dir.rglob("*.gcda"))


def reset_gcda(build_dir: Path) -> int:
    """Delete stale .gcda counters so the next test run measures only itself.

    Lesson: without clearing .gcda, running fewer tests still shows the old
    (possibly full) coverage, because counters accumulate across runs.
    """
    removed = 0
    for gcda in find_gcda_files(build_dir):
        gcda.unlink()
        removed += 1
    print(f"removed {removed} stale .gcda file(s) under {build_dir}")
    return 0


def measure_coverage(build_dir: Path, repo_root: Path) -> tuple[int, int]:
    """Run gcov over the build tree; count covered/total lines for src/ files."""
    gcov = shutil.which("gcov")
    if gcov is None:
        raise RuntimeError("gcov not found on PATH")

    src_root = (repo_root / "src").resolve()
    covered = total = 0
    with tempfile.TemporaryDirectory(prefix="coverage-ratchet-") as tmp:
        for gcda in find_gcda_files(build_dir):
            result = subprocess.run(
                [gcov, "--preserve-paths", "--object-directory",
                 str(gcda.parent), str(gcda)],
                capture_output=True, encoding="utf-8", errors="replace",
                timeout=300, cwd=tmp,
            )
            if result.returncode != 0:
                continue
            for gcov_file in Path(tmp).rglob("*.gcov"):
                source = _gcov_source_path(gcov_file)
                if source is None:
                    continue
                try:
                    resolved = Path(source).resolve()
                except OSError:
                    continue
                if not _is_within(resolved, src_root):
                    continue
                c, t = _count_gcov_lines(gcov_file)
                covered += c
                total += t
                gcov_file.unlink()
    return covered, total


def _gcov_source_path(gcov_file: Path) -> str | None:
    """Extract the source path from a .gcov header line.

    gcov emits it as ``        -:    0:Source:/path/to/file.c`` (three
    colon-separated prefix fields, then the record).
    """
    try:
        with gcov_file.open(encoding="utf-8", errors="replace") as handle:
            for line in handle:
                if ":Source:" in line:
                    return line.split(":Source:", 1)[1].strip()
                if ":Graph:" in line:
                    break  # header passed without a Source record
    except OSError:
        return None
    return None


def _is_within(path: Path, root: Path) -> bool:
    try:
        path.relative_to(root)
        return True
    except ValueError:
        return False


def _count_gcov_lines(gcov_file: Path) -> tuple[int, int]:
    covered = total = 0
    with gcov_file.open(encoding="utf-8", errors="replace") as handle:
        for line in handle:
            match = _GCOV_LINE_RE.match(line)
            if not match:
                continue
            count = match.group(1)
            if count == "-":
                continue  # non-executable line
            total += 1
            if count != "#####":
                covered += 1
    return covered, total


def read_baseline(baseline_path: Path) -> float | None:
    if not baseline_path.is_file():
        return None
    text = baseline_path.read_text(encoding="utf-8").strip()
    match = re.search(r"(\d+(?:\.\d+)?)", text)
    return float(match.group(1)) if match else None


def write_baseline(baseline_path: Path, percent: float) -> None:
    # Store with enough precision that a re-measurement of the same tree
    # does not read as "below baseline" due to display rounding.
    baseline_path.write_text(f"{percent:.6f}\n", encoding="utf-8")


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(
        description="Coverage ratchet: line coverage may only go up.")
    parser.add_argument("build_dir", type=Path,
                        help="coverage-instrumented build tree (e.g. "
                             "build/ninja-coverage)")
    parser.add_argument("--repo-root", type=Path,
                        default=Path(__file__).resolve().parents[1])
    parser.add_argument("--baseline-file", type=Path, default=None,
                        help=f"default: scripts/{_BASELINE_FILENAME}")
    parser.add_argument("--update", action="store_true",
                        help="write the measured value as the new baseline "
                             "(only upward moves are allowed)")
    parser.add_argument("--reset-gcda", action="store_true",
                        help="delete stale .gcda counters and exit (run this "
                             "before re-running the tests)")
    args = parser.parse_args(argv[1:])

    repo_root: Path = args.repo_root.resolve()
    build_dir: Path = args.build_dir
    if not build_dir.is_dir():
        _fail(f"构建目录不存在: {build_dir}")
        return 1

    if args.reset_gcda:
        return reset_gcda(build_dir)

    baseline_path = args.baseline_file or (
        Path(__file__).resolve().parent / _BASELINE_FILENAME)

    try:
        covered, total = measure_coverage(build_dir, repo_root)
    except RuntimeError as exc:
        _fail(str(exc))
        return 1

    if total == 0:
        _fail(f"覆盖率测量为 0 行可执行代码——{build_dir} 可能不是覆盖率构建，"
              "或测试尚未运行（先 --reset-gcda 再跑 ctest）")
        return 1

    percent = 100.0 * covered / total
    print(f"measured: {covered}/{total} lines = {percent:.2f}% (src/ only)")

    baseline = read_baseline(baseline_path)
    if baseline is None:
        if args.update:
            write_baseline(baseline_path, percent)
            print(f"baseline written: {percent:.2f}% -> {baseline_path}")
            return 0
        _fail(f"没有基线文件 {baseline_path}；先用 --update 写入首个基线")
        return 1

    print(f"baseline: {baseline:.2f}% ({baseline_path})")
    if percent + 1e-6 < baseline:
        _fail(f"覆盖率下降：{percent:.2f}% < 基线 {baseline:.2f}%")
        return 1

    if args.update and percent > baseline:
        write_baseline(baseline_path, percent)
        print(f"baseline ratcheted up: {baseline:.2f}% -> {percent:.2f}%")
    elif percent >= baseline:
        print(f"OK: coverage {percent:.2f}% >= baseline {baseline:.2f}%")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))

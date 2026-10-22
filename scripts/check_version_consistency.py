#!/usr/bin/env python3
"""Check that the project version agrees across its four sources of truth.

Sources compared:

1. ``CMakeLists.txt`` — the ``project(... VERSION x.y.z ...)`` declaration.
2. The generated version header — ``<UPPER>_VERSION_STRING "x.y.z"`` produced
   by CMake ``configure_file`` into the build tree.
3. The built CLI — ``<exe> --version`` output.
4. ``CHANGELOG.md`` — the newest ``## x.y.z`` section heading.

Documentation under ``docs/`` is deliberately NOT scanned: guides contain
example ``project(... VERSION ...)`` snippets that would produce false
positives.

Usage
-----
    python3 scripts/check_version_consistency.py <repo_root> \
        --build-dir <build_dir> --exe <path-to-cli-executable>

The project name is parsed from ``CMakeLists.txt``; nothing about a specific
project name or prefix is hard-coded.

Exit code: 0 = all four sources agree; 1 = any mismatch or parse failure.
"""

import argparse
import re
import subprocess
import sys
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


_VERSION_RE = re.compile(r"\b(\d+)\.(\d+)\.(\d+)\b")


def _fail(message: str) -> None:
    print(f"FAIL: {message}", file=sys.stderr)


def read_cmake_version(cmakelists: Path) -> tuple[str, str] | None:
    """Return (project_name, version) from the project() command, or None."""
    text = cmakelists.read_text(encoding="utf-8", errors="replace")
    match = re.search(
        r"\bproject\s*\(\s*([A-Za-z_][A-Za-z0-9_]*)\s+VERSION\s+(\d+\.\d+\.\d+)",
        text,
        re.IGNORECASE,
    )
    if match:
        return match.group(1), match.group(2)
    # project(name ...) without inline VERSION, then a separate VERSION is rare;
    # fall back to a loose two-step parse.
    name_match = re.search(r"\bproject\s*\(\s*([A-Za-z_][A-Za-z0-9_]*)", text)
    ver_match = re.search(r"\bVERSION\s+(\d+\.\d+\.\d+)", text)
    if name_match and ver_match:
        return name_match.group(1), ver_match.group(1)
    return None


def find_generated_header(build_dir: Path, project_name: str) -> Path | None:
    """Locate the generated version header under build_dir.

    The macro prefix derives from the project's lowercase namespace (which may
    differ from the PascalCase project name), so the header is identified by
    shape: a ``version.h`` that defines ``<UPPER>_VERSION_STRING``. Nothing
    project-specific is hard-coded.
    """
    for candidate in sorted(build_dir.rglob("version.h")):
        text = candidate.read_text(encoding="utf-8", errors="replace")
        if re.search(r"\b[A-Z][A-Z0-9_]*_VERSION_STRING\s+\"", text):
            return candidate
    return None


def read_header_version(header: Path, project_name: str) -> str | None:
    text = header.read_text(encoding="utf-8", errors="replace")
    match = re.search(r"\b[A-Z][A-Z0-9_]*_VERSION_STRING\s+\"([^\"]+)\"", text)
    return match.group(1) if match else None


def read_cli_version(exe: Path) -> str | None:
    try:
        result = subprocess.run(
            [str(exe), "--version"],
            capture_output=True,
            encoding="utf-8",
            errors="replace",
            timeout=30,
        )
    except (OSError, subprocess.TimeoutExpired) as exc:
        _fail(f"无法运行 {exe} --version: {exc}")
        return None
    output = result.stdout + result.stderr
    match = _VERSION_RE.search(output)
    return match.group(0) if match else None


def read_changelog_version(changelog: Path) -> str | None:
    text = changelog.read_text(encoding="utf-8", errors="replace")
    match = re.search(r"^##\s+(\d+\.\d+\.\d+)", text, re.MULTILINE)
    return match.group(1) if match else None


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(
        description="Check that the project version agrees across its four sources of truth.")
    parser.add_argument("repo_root", type=Path)
    parser.add_argument("--build-dir", type=Path, required=True,
                        help="CMake build tree containing the generated header")
    parser.add_argument("--exe", type=Path, required=True,
                        help="built CLI executable to query with --version")
    args = parser.parse_args(argv[1:])

    root: Path = args.repo_root
    cmakelists = root / "CMakeLists.txt"
    changelog = root / "CHANGELOG.md"

    problems: list[str] = []

    if not cmakelists.is_file():
        _fail(f"缺少 {cmakelists}")
        return 1
    parsed = read_cmake_version(cmakelists)
    if parsed is None:
        _fail(f"{cmakelists}: 无法解析 project(... VERSION x.y.z)")
        return 1
    project_name, cmake_version = parsed

    if not args.build_dir.is_dir():
        _fail(f"构建目录不存在: {args.build_dir}")
        return 1
    header = find_generated_header(args.build_dir, project_name)
    header_version = None
    if header is None:
        problems.append(f"{args.build_dir}: 未找到生成的版本头文件（含 <UPPER>_VERSION_STRING 的 version.h）")
    else:
        header_version = read_header_version(header, project_name)
        if header_version is None:
            problems.append(f"{header}: 未找到 <UPPER>_VERSION_STRING")

    if not args.exe.is_file():
        problems.append(f"可执行文件不存在: {args.exe}")
        cli_version = None
    else:
        cli_version = read_cli_version(args.exe)
        if cli_version is None:
            problems.append(f"{args.exe} --version: 输出中没有 x.y.z 版本号")

    if not changelog.is_file():
        problems.append(f"缺少 {changelog}")
        changelog_version = None
    else:
        changelog_version = read_changelog_version(changelog)
        if changelog_version is None:
            problems.append(f"{changelog}: 未找到 '## x.y.z' 段落")

    sources = {
        "CMakeLists.txt project VERSION": cmake_version,
        f"generated header ({header.name if header else '?'}): "
        "<UPPER>_VERSION_STRING": header_version,
        f"{args.exe.name} --version": cli_version,
        "CHANGELOG.md newest section": changelog_version,
    }

    for problem in problems:
        _fail(problem)

    known = {label: value for label, value in sources.items() if value}
    distinct = sorted(set(known.values()))
    if len(distinct) != 1 or problems:
        print("版本不一致：", file=sys.stderr)
        for label, value in sources.items():
            print(f"  {label}: {value if value else '<缺失>'}", file=sys.stderr)
        return 1

    print(f"OK: version {distinct[0]} consistent across "
          f"CMakeLists.txt, generated header, CLI --version, and CHANGELOG.md")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))

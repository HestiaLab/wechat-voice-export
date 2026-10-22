#!/usr/bin/env python3
"""CHANGELOG <-> git tag consistency gate.

Checks (a version section is ``## x.y.z`` optionally followed by a date):

1. **Section shape** — every ``##`` section in ``CHANGELOG.md`` must be a bare
   version heading (``## x.y.z``, an optional `` — date`` suffix is allowed).
   A section whose heading is not a version fails.
2. **Every tag has a section** — each ``v*`` tag must have a same-version
   section in ``CHANGELOG.md``.
3. **Released sections are frozen** — for a version that *has* a tag, the
   section text at the tag must be byte-identical to the section text in the
   working tree. Rewriting a released section is a failure that names the
   version and the tag.

Sections whose version has **no** tag (in-development / unreleased) are not
frozen: the byte-identity comparison is skipped for them, and the skip is
reported explicitly with the note that **跳过 ≠ 通过** (a skip is not a pass —
it means no evidence was gathered, not that the content is correct).

Git context is REQUIRED. If this is not a git working copy, or git cannot
produce tag/section evidence, the gate reports 「无法取证」 and exits 1 — it
never degrades to a silent skip, because a skipped check that prints OK is
indistinguishable from a passed one.

Usage
-----
    python3 scripts/check_changelog_tag.py <repo_root>

Exit code: 0 = all checks passed (skips reported, none fatal); 1 = any
inconsistency, or git evidence unavailable.
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


_SECTION_RE = re.compile(r"^##\s+(?P<version>\d+\.\d+\.\d+)\s*(?:—|--|-)?\s*(?P<rest>.*)$")
_ANY_H2_RE = re.compile(r"^##\s+(?P<text>.*)$")
_TAG_RE = re.compile(r"^v(\d+\.\d+\.\d+)$")


def _fail(message: str, problems: list[str]) -> None:
    problems.append(message)


def run_git(repo: Path, *args: str) -> tuple[int, str, str]:
    """Run git and decode its output explicitly as UTF-8.

    ``git show <rev>:<path>`` returns the blob verbatim (unlike log output it
    is not transcoded), and this repository is UTF-8 by ``.gitattributes``.
    Letting subprocess guess the locale - GBK on a Chinese Windows host,
    cp1252 on the runners - raises UnicodeDecodeError inside the reader
    thread and leaves stdout as ``None``, i.e. the gate crashes instead of
    returning a verdict.
    """
    result = subprocess.run(
        ["git", *args], cwd=repo, capture_output=True,
        encoding="utf-8", errors="replace", timeout=120)
    return result.returncode, result.stdout, result.stderr


def git_evidence_available(repo: Path) -> bool:
    code, out, _ = run_git(repo, "rev-parse", "--is-inside-work-tree")
    return code == 0 and out.strip() == "true"


def git_is_shallow(repo: Path) -> bool:
    code, out, _ = run_git(repo, "rev-parse", "--is-shallow-repository")
    return code == 0 and out.strip() == "true"


def list_version_tags(repo: Path) -> list[str]:
    """Return sorted v* tag version strings that look like x.y.z."""
    code, out, _ = run_git(repo, "tag", "-l", "v*")
    if code != 0:
        return []
    versions = []
    for line in out.splitlines():
        match = _TAG_RE.match(line.strip())
        if match:
            versions.append(match.group(1))
    return sorted(versions)


def parse_sections(changelog: Path) -> tuple[list[tuple[str, int, str]], list[str]]:
    """Return (sections, shape_problems).

    sections: list of (version, start_line, section_text_including_heading).
    A section runs from its ``##`` heading up to (not including) the next
    ``##`` heading or EOF.
    """
    lines = changelog.read_text(encoding="utf-8", errors="replace").splitlines()
    sections: list[tuple[str, int, str]] = []
    problems: list[str] = []
    current: tuple[str, int, list[str]] | None = None

    for lineno, line in enumerate(lines, start=1):
        h2 = _ANY_H2_RE.match(line)
        if h2:
            if current is not None:
                sections.append((current[0], current[1], "\n".join(current[2])))
                current = None
            ver = _SECTION_RE.match(line)
            if ver is None:
                problems.append(
                    f"{changelog.name}:{lineno}: 段落标题不是版本号形态: "
                    f"'{h2.group('text').strip()}'（应为 '## x.y.z'，可带 ' — 日期' 后缀）")
                continue
            current = (ver.group("version"), lineno, [line])
        elif current is not None:
            current[2].append(line)

    if current is not None:
        sections.append((current[0], current[1], "\n".join(current[2])))
    return sections, problems


def section_at_tag(repo: Path, changelog_rel: str, tag: str) -> tuple[str, str] | None:
    """Return (section_text, raw_file_text) for the version at `tag`, or None."""
    version = tag[1:] if tag.startswith("v") else tag
    code, out, _ = run_git(repo, "show", f"{tag}:{changelog_rel}")
    if code != 0:
        return None
    tag_path = repo / changelog_rel
    lines = out.splitlines()
    collected: list[str] = []
    in_section = False
    for line in lines:
        if _ANY_H2_RE.match(line):
            if in_section:
                break
            ver = _SECTION_RE.match(line)
            if ver and ver.group("version") == version:
                in_section = True
                collected.append(line)
                continue
        elif in_section:
            collected.append(line)
    if not in_section:
        return None
    del tag_path
    return "\n".join(collected), out


def normalize(text: str) -> str:
    """Compare ignoring trailing blank lines and line-ending style."""
    return text.replace("\r\n", "\n").rstrip("\n")


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(
        description="CHANGELOG <-> git tag consistency gate.")
    parser.add_argument("repo_root", type=Path)
    args = parser.parse_args(argv[1:])

    root: Path = args.repo_root.resolve()
    if not root.is_dir():
        print(f"FAIL: 目录不存在: {root}", file=sys.stderr)
        return 1

    changelog = root / "CHANGELOG.md"
    if not changelog.is_file():
        print(f"FAIL: 缺少 {changelog}", file=sys.stderr)
        return 1

    if not git_evidence_available(root):
        print("FAIL: 无法取证——该目录不是 git 工作副本，"
              "CHANGELOG↔tag 一致性无法验证（不降级为跳过）", file=sys.stderr)
        return 1
    if git_is_shallow(root):
        # A shallow clone does not fetch tags, so "no tags" here would be a
        # vacuous pass on a repository that actually has releases.
        print("FAIL: 无法取证——浅克隆不会取到 tag，"
              "「无 tag 段落」结论是假绿；请用完整克隆"
              "（CI 需 fetch-depth: 0）（不降级为跳过）", file=sys.stderr)
        return 1

    problems: list[str] = []
    skips: list[str] = []

    sections, shape_problems = parse_sections(changelog)
    for problem in shape_problems:
        _fail(problem, problems)

    if not sections:
        _fail(f"{changelog.name}: 没有任何 '## x.y.z' 版本段落（扫描面为空）",
              problems)

    section_by_version = {version: (lineno, text) for version, lineno, text in sections}

    tags = list_version_tags(root)
    for tag_version in tags:
        if tag_version not in section_by_version:
            _fail(f"{changelog.name}: tag v{tag_version} 没有对应的版本段落",
                  problems)
            continue
        lineno, worktree_text = section_by_version[tag_version]
        at_tag = section_at_tag(root, changelog.name, f"v{tag_version}")
        if at_tag is None:
            _fail(f"{changelog.name}:{lineno}: 无法取证——tag v{tag_version} 处"
                  f"读不到该版本段落（不降级为跳过）", problems)
            continue
        tag_text, _ = at_tag
        if normalize(tag_text) != normalize(worktree_text):
            _fail(f"{changelog.name}:{lineno}: 已发布段落被改写——v{tag_version} "
                  f"版本段落与 tag 处不逐字一致（已发布段落不可改写）", problems)

    for version, (lineno, _text) in sorted(section_by_version.items()):
        if version not in tags:
            skips.append(f"{changelog.name}:{lineno}: 版本 {version} 无对应 tag，"
                         "跳过冻结比对（跳过 ≠ 通过：未取证，不代表内容正确）")

    for skip in skips:
        print(f"SKIP: {skip}")
    if problems:
        for problem in problems:
            print(f"ERROR: {problem}", file=sys.stderr)
        print(f"FAIL: CHANGELOG↔tag 门禁发现 {len(problems)} 处问题", file=sys.stderr)
        return 1

    print(f"OK: {len(sections)} 个版本段落，{len(tags)} 个 v* tag，"
          f"冻结比对全部逐字一致（{len(skips)} 处跳过）")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))

#!/usr/bin/env python3
"""Documentation gate: link/anchor reachability, fence pairing, index coverage.

Sub-checks (``--mode``):

``links`` (B1)
    - Every relative Markdown link target in ``*.md`` files must exist on disk
      (paths are URL-decoded first; ``%20`` is a legal spelling). ``build/``
      and ``.git/`` are excluded. External URLs (http/https/mailto) are not
      fetched and not checked.
    - Every in-page anchor (``#foo``) and cross-file anchor (``file.md#foo``)
      must match a heading in the target file, where the anchor is derived
      from the real heading text with one shared slug rule (GitHub-style:
      lowercase, punctuation dropped except hyphens/underscores/CJK, spaces
      to hyphens). Hand-typed anchors that do not match any heading fail.
    - Code fences (``` and ~~~) must be paired in every file.

Scan surface
------------
Only files git would show for the repository (tracked ∪ untracked-not-ignored)
are judged, with ``build/`` and ``.git/`` excluded. A locally ignored scratch
directory must not be able to flip a verdict, or the same commit would answer
differently on a developer machine and in a CI checkout. Outside a git work
tree the plain on-disk walk is used, since there is no ignore semantics to
honour.

``index`` (B3')
    - Bidirectional comparison between the documentation index in the root
      ``README.md`` (its docs/ links) and the set of ``docs/**/*.md`` files:
      no orphan document (every file under docs/ is linked from the README
      index) and no dead index entry (every docs-relative link in the index
      resolves).

Deliberately NOT checked: changelog-table continuity (template docs are not
version-controlled documents).

Usage
-----
    python3 scripts/check_docs.py <repo_root> [--mode links|index|all]

Exit code: 0 = clean; 1 = at least one violation, each printed as
``file:line: message``.
"""

import argparse
import re
import subprocess
import sys
import urllib.parse
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


_EXCLUDED_DIRS = {"build", ".git"}
_LINK_RE = re.compile(r"(?<!!)\[[^\]]*\]\(([^)\s]+)\)")
_FENCE_RE = re.compile(r"^\s*(```|~~~)")
_HEADING_RE = re.compile(r"^(#{1,6})\s+(.*)$")


def _fail(location: str, message: str, problems: list[str]) -> None:
    problems.append(f"{location}: {message}")


def git_live_paths(root: Path) -> set[str] | None:
    """Repo-relative paths git would show: tracked ∪ untracked-not-ignored.

    The gate judges the repository, not whatever happens to sit in the working
    directory. Locally ignored scratch (``.hermes/``, ``dist/``) must not be
    able to flip a verdict, otherwise the same commit answers differently on a
    developer machine and in a CI checkout. Returns ``None`` outside a git work
    tree, where there is no ignore semantics to honour and a plain on-disk walk
    is the only available definition.
    """
    try:
        inside = subprocess.run(
            ["git", "rev-parse", "--is-inside-work-tree"], cwd=root,
            capture_output=True, encoding="utf-8", errors="replace", timeout=60)
        if inside.returncode != 0 or inside.stdout.strip() != "true":
            return None
        listing = subprocess.run(
            ["git", "ls-files", "-z", "--cached", "--others",
             "--exclude-standard"], cwd=root,
            capture_output=True, encoding="utf-8", errors="replace", timeout=300)
    except (OSError, subprocess.TimeoutExpired):
        return None
    if listing.returncode != 0:
        return None
    return {entry for entry in listing.stdout.split("\0") if entry}


def iter_markdown_files(root: Path):
    live = git_live_paths(root)
    for path in sorted(root.rglob("*.md")):
        rel = path.relative_to(root)
        if any(part in _EXCLUDED_DIRS for part in rel.parts):
            continue
        if live is not None and rel.as_posix() not in live:
            continue
        yield path


def split_code_spans(text_lines: list[str]) -> list[tuple[int, str, bool]]:
    """Return (lineno, line, in_fence) for each line of a file."""
    result = []
    in_fence = False
    fence_char = ""
    for lineno, line in enumerate(text_lines, start=1):
        match = _FENCE_RE.match(line)
        if match:
            marker = match.group(1)
            if not in_fence:
                in_fence = True
                fence_char = marker
                result.append((lineno, line, True))
                continue
            if marker == fence_char:
                in_fence = False
                fence_char = ""
        result.append((lineno, line, in_fence))
    return result


def github_slug(heading_text: str) -> str:
    """GitHub-style heading slug: lowercase, punctuation dropped (except
    hyphen/underscore and non-ASCII word characters), spaces to hyphens."""
    text = heading_text.strip()
    text = re.sub(r"[`*_]", "", text)  # inline markup characters
    text = text.lower()
    out = []
    for ch in text:
        if ch.isalnum() or ch in "-_" or ord(ch) > 127:
            out.append(ch)
        elif ch.isspace():
            out.append("-")
        # everything else (punctuation) is dropped
    return "".join(out)


def collect_anchors(lines: list[str]) -> set[str]:
    """Anchors for every real heading (headings inside code fences don't count)."""
    anchors: set[str] = set()
    for _lineno, line, in_fence in split_code_spans(lines):
        if in_fence:
            continue
        match = _HEADING_RE.match(line)
        if match:
            slug = github_slug(match.group(2))
            if slug:
                anchors.add(slug)
    return anchors


def check_fences(root: Path, path: Path, lines: list[str],
                 problems: list[str]) -> None:
    annotated = split_code_spans(lines)
    # A file is fence-balanced iff the last line is not inside a fence.
    if annotated and annotated[-1][2]:
        _fail(str(path.relative_to(root)), "代码围栏未配对（文件结尾仍在围栏内）",
              problems)


def check_links(root: Path, all_files: list[Path], problems: list[str]) -> None:
    anchor_cache: dict[Path, set[str]] = {}
    for path in all_files:
        lines = path.read_text(encoding="utf-8",
                               errors="replace").splitlines()
        check_fences(root, path, lines, problems)
        for lineno, line, in_fence in split_code_spans(lines):
            if in_fence:
                continue
            for match in _LINK_RE.finditer(line):
                target = match.group(1).strip()
                if target.startswith(("http://", "https://", "mailto:")):
                    continue
                location = f"{path.relative_to(root)}:{lineno}"
                decoded = urllib.parse.unquote(target)
                if decoded.startswith("#"):
                    if path not in anchor_cache:
                        anchor_cache[path] = collect_anchors(lines)
                    anchor = decoded[1:]
                    if anchor and anchor not in anchor_cache[path]:
                        _fail(location,
                              f"页内锚点不可达: #{anchor}（没有匹配的标题）",
                              problems)
                    continue
                file_part, _, frag = decoded.partition("#")
                if not file_part:
                    continue
                resolved = (path.parent / file_part).resolve()
                if not resolved.exists():
                    _fail(location, f"相对链接目标不存在: {file_part}", problems)
                    continue
                if frag:
                    if resolved.is_file() and resolved.suffix == ".md":
                        if resolved not in anchor_cache:
                            anchor_cache[resolved] = collect_anchors(
                                resolved.read_text(encoding="utf-8",
                                                   errors="replace")
                                .splitlines())
                        if frag not in anchor_cache[resolved]:
                            _fail(location,
                                  f"跨文件锚点不可达: {file_part}#{frag}",
                                  problems)


def check_index(root: Path, problems: list[str]) -> None:
    docs_root = root / "docs"
    index_path = root / "README.md"
    if not docs_root.is_dir():
        _fail(str(root), "docs/ 目录不存在", problems)
        return
    if not index_path.is_file():
        _fail(str(root), "缺少文档索引 README.md", problems)
        return

    actual = {path.resolve() for path in iter_markdown_files(docs_root)}
    if not actual:
        _fail(str(docs_root), "docs/ 下没有任何文档（扫描面为空）", problems)
        return

    linked: set[Path] = set()
    index_lines = index_path.read_text(encoding="utf-8",
                                       errors="replace").splitlines()
    for lineno, line, in_fence in split_code_spans(index_lines):
        if in_fence:
            continue
        for match in _LINK_RE.finditer(line):
            target = urllib.parse.unquote(match.group(1).strip())
            if target.startswith(("http://", "https://", "mailto:")):
                continue
            file_part = target.partition("#")[0]
            if not file_part:
                continue
            resolved = (index_path.parent / file_part).resolve()
            location = f"README.md:{lineno}"
            # Only links that point into docs/ belong to the index contract:
            # a dead docs/ link is an index defect; other relative links are
            # already covered by the links mode.
            if docs_root.resolve() not in resolved.parents:
                continue
            if not resolved.exists():
                _fail(location, f"索引死链: {file_part}", problems)
                continue
            linked.add(resolved)

    orphans = sorted(actual - linked)
    for orphan in orphans:
        _fail(str(orphan.relative_to(root)),
              "孤儿文档: README.md 文档索引未引用该文件", problems)


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(
        description="Documentation gate: links, anchors, fences, index.")
    parser.add_argument("repo_root", type=Path)
    parser.add_argument("--mode", choices=("links", "index", "all"),
                        default="all")
    args = parser.parse_args(argv[1:])

    root: Path = args.repo_root.resolve()
    if not root.is_dir():
        print(f"FAIL: 目录不存在: {root}", file=sys.stderr)
        return 1

    all_files = list(iter_markdown_files(root))
    problems: list[str] = []

    if args.mode in ("links", "all"):
        if not all_files:
            _fail(str(root), "解析失败：扫描面内没有任何 .md 文件", problems)
        check_links(root, all_files, problems)
    if args.mode in ("index", "all"):
        check_index(root, problems)

    if problems:
        for problem in problems:
            print(f"ERROR: {problem}", file=sys.stderr)
        print(f"FAIL: docs 门禁发现 {len(problems)} 处问题", file=sys.stderr)
        return 1
    print(f"OK: docs 门禁通过（mode={args.mode}，扫描 {len(all_files)} 个 .md 文件）")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))

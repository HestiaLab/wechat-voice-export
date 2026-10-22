#!/usr/bin/env python3
"""Backtick path reachability gate (git view).

Every backtick-quoted, path-shaped token in living Markdown docs must exist
from the **git point of view**: tracked (index) ∪ ignored ∪ history union.
A path that exists on disk but is NOT in git is reported explicitly — docs
that reference uncommitted files break for everyone else.

Token selection (documented criteria, no project names hard-coded):

- The token contains ``/`` and matches ``[A-Za-z0-9._][A-Za-z0-9._/+-]*``
  (so flags like ``--enable-tidy``, angles like ``include/<project>/x.h``,
  and prose with spaces are not paths).
- Anchoring: a token is resolved against the repo root first, then against
  the containing document's directory.
- First-segment filter: the token's first path segment must exist (as a name)
  at one of the anchors, on disk or in the git view. This excludes install-tree
  and build-tree layout references (e.g. ``bin/``, ``lib/cmake/<Project>/``,
  ``share/<project>/`` in package-contents tables, ``<ns>/version.h`` include
  paths) that never live in the repository. Deletion drift is NOT hidden by
  this filter: a removed top-level directory remains in the history union, so
  references into it stay candidates and resolve via git-view.

Git I/O uses bytes and ``-z`` (NUL-separated) throughout: on Windows, text
mode would translate ``\\n`` to ``\\r\\n`` in stdin and make
``git check-ignore --stdin`` match nothing.

Git context is REQUIRED: in a non-git directory the gate reports 无法取证 and
exits 1 — it never degrades to a silent skip (跳过 ≠ 通过).

Scan surface: the Markdown files git would show for the repository (tracked ∪
untracked-not-ignored, ``build/`` and ``.git/`` excluded). Ignored local
scratch (``.hermes/``, ``dist/``) must not be able to flip a verdict, or the
same commit would answer differently on a developer machine and in a CI
checkout.

Usage
-----
    python3 scripts/check_doc_paths.py <repo_root>

Exit code: 0 = clean; 1 = any unreachable / untracked reference, empty scan
surface, or missing git evidence.
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


_EXCLUDED_DIRS = {"build", ".git"}
_BACKTICK_RE = re.compile(r"`([^`\n]+)`")
_PATH_TOKEN_RE = re.compile(r"^[A-Za-z0-9._][A-Za-z0-9._/+-]*$")
# Scan surface floor: a docs gate that finds zero path tokens in a repo with
# Markdown files means the tokenizer broke, not that the docs are clean.
_MIN_TOKENS = 10  # measured 48 on this template 2026-10-10, x ~0.2 margin


def _fail(message: str, problems: list[str]) -> None:
    problems.append(message)


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


def git_bytes(repo: Path, *args: str, stdin: bytes | None = None) -> tuple[int, bytes]:
    result = subprocess.run(
        ["git", *args], cwd=repo, input=stdin, capture_output=True, timeout=300)
    return result.returncode, result.stdout


def git_evidence_available(repo: Path) -> bool:
    code, out = git_bytes(repo, "rev-parse", "--is-inside-work-tree")
    return code == 0 and out.strip() == b"true"


def git_is_shallow(repo: Path) -> bool:
    code, out = git_bytes(repo, "rev-parse", "--is-shallow-repository")
    return code == 0 and out.strip() == b"true"


def git_view_paths(repo: Path) -> set[str]:
    """tracked (index) ∪ history union, as repo-relative posix strings."""
    view: set[str] = set()
    for args in (
        ("ls-files", "-z"),
        ("log", "--all", "--pretty=tformat:", "--name-only", "-z"),
    ):
        code, out = git_bytes(repo, *args)
        if code != 0:
            continue
        for raw in out.split(b"\0"):
            if raw:
                view.add(raw.decode("utf-8", "replace"))
    return view


def ignored_set(repo: Path, candidates: list[str]) -> set[str]:
    """Batch check-ignore with NUL-separated bytes I/O (Windows-safe)."""
    if not candidates:
        return set()
    stdin = b"\0".join(c.encode("utf-8") for c in candidates) + b"\0"
    code, out = git_bytes(repo, "check-ignore", "--stdin", "-z", stdin=stdin)
    # exit 0 = some paths ignored; 1 = none ignored; >1 = error
    if code > 1:
        return set()
    return {raw.decode("utf-8", "replace") for raw in out.split(b"\0") if raw}


def strip_fenced(text: str) -> list[tuple[int, str]]:
    """Return (lineno, line) for lines outside fenced code blocks."""
    out = []
    in_fence = False
    for lineno, line in enumerate(text.splitlines(), start=1):
        if line.strip().startswith("```") or line.strip().startswith("~~~"):
            in_fence = not in_fence
            continue
        if not in_fence:
            out.append((lineno, line))
    return out


def iter_docs(root: Path):
    live = git_live_paths(root)
    for path in sorted(root.rglob("*.md")):
        rel = path.relative_to(root)
        if any(part in _EXCLUDED_DIRS for part in rel.parts):
            continue
        if live is not None and rel.as_posix() not in live:
            continue
        yield path


def collect_tokens(root: Path) -> list[tuple[str, str, int, str]]:
    """(token, doc_relpath, lineno, raw) for every path-shaped backtick token."""
    found = []
    for path in iter_docs(root):
        rel = str(path.relative_to(root))
        text = path.read_text(encoding="utf-8", errors="replace")
        for lineno, line in strip_fenced(text):
            for match in _BACKTICK_RE.finditer(line):
                token = match.group(1).strip()
                if "/" not in token:
                    continue
                if token.startswith(("-", "/")):
                    continue
                if not _PATH_TOKEN_RE.match(token):
                    continue
                found.append((token.rstrip("/"), rel, lineno, token))
    return found


def anchor_candidates(root: Path, token: str, doc_rel: str):
    """Yield (anchor_name, repo_relative_posix, abs_path) for each anchor."""
    doc_dir = (root / doc_rel).parent
    for name, base in (("repo-root", root), ("doc-dir", doc_dir)):
        abs_path = (base / token).resolve()
        try:
            rel = abs_path.relative_to(root.resolve())
        except ValueError:
            continue  # escaped the repo via ../ — not a repo reference
        yield name, rel.as_posix(), abs_path


def first_segment_known(root: Path, token: str, doc_rel: str,
                        git_view: set[str]) -> bool:
    """Does the token's first segment exist at some anchor (disk or git view)?

    Top-level names in the git view are cheap to derive; doc-dir level names
    are checked on disk and in the git view under the doc's directory.
    """
    first = token.split("/", 1)[0]
    if (root / first).exists():
        return True
    if first in {p.split("/", 1)[0] for p in git_view}:
        return True
    doc_dir_rel = str((root / doc_rel).parent.relative_to(root))
    if doc_dir_rel != ".":
        if (root / doc_dir_rel / first).exists():
            return True
        prefix = doc_dir_rel + "/"
        if first in {p[len(prefix):].split("/", 1)[0]
                     for p in git_view if p.startswith(prefix)}:
            return True
    return False


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(
        description="Backtick path reachability gate (git view).")
    parser.add_argument("repo_root", type=Path)
    args = parser.parse_args(argv[1:])

    root: Path = args.repo_root.resolve()
    if not root.is_dir():
        print(f"FAIL: 目录不存在: {root}", file=sys.stderr)
        return 1
    if not git_evidence_available(root):
        print("FAIL: 无法取证——该目录不是 git 工作副本，"
              "反引号路径的 git 视角判定无法进行（不降级为跳过）", file=sys.stderr)
        return 1
    if git_is_shallow(root):
        print("FAIL: 无法取证——浅克隆的历史并集不完整，"
              "「git 视角下不存在」的判定不可靠；请用完整克隆"
              "（CI 需 fetch-depth: 0）（不降级为跳过）", file=sys.stderr)
        return 1

    tokens = collect_tokens(root)
    problems: list[str] = []
    if not tokens:
        _fail(f"{root}: 解析失败：活文档中没有任何反引号路径（扫描面为空）",
              problems)
    if 0 < len(tokens) < _MIN_TOKENS:
        _fail(f"{root}: 解析失败：反引号路径仅 {len(tokens)} 项"
              f"（下限 {_MIN_TOKENS}）——检查扫描范围或分词规则是否变化",
              problems)

    git_view = git_view_paths(root)
    if not git_view:
        _fail(f"{root}: 无法取证——git 视角为空（无 tracked 且无历史）", problems)

    # Resolve each token at its anchors; classify by best evidence.
    unresolved_rel: set[str] = set()
    located: dict[tuple[str, str], tuple[str, str]] = {}
    skipped: list[str] = []
    for token, doc_rel, lineno, _raw in tokens:
        key = (token, doc_rel)
        if key in located:
            continue
        best = None
        for _anchor, rel, abs_path in anchor_candidates(root, token, doc_rel):
            in_view = rel in git_view or any(
                f.startswith(rel + "/") for f in git_view)
            on_disk = abs_path.exists()
            if in_view:
                best = ("git-view", rel)
                break
            if on_disk:
                best = ("untracked", rel)  # keep looking for a git-view anchor
                unresolved_rel.add(rel)
            elif best is None:
                best = ("nowhere", rel)
                unresolved_rel.add(rel)
        if best is None:
            continue  # escaped the repo via ../ at every anchor
        if best[0] == "nowhere" and not first_segment_known(
                root, token, doc_rel, git_view):
            skipped.append(f"{doc_rel}:{lineno}: `{token}` 非仓库锚定路径"
                           "（首段在任何锚点的盘上/git 视角都不存在，"
                           "判为安装树/构建树布局引用）")
            continue
        located[key] = best

    ignored = ignored_set(root, sorted(unresolved_rel))

    reported: set[str] = set()
    for token, doc_rel, lineno, _raw in tokens:
        key = (token, doc_rel)
        if key not in located:
            continue
        status, rel = located[key]
        if status == "git-view" or rel in ignored:
            continue
        dedupe = f"{doc_rel}:{lineno}:{rel}"
        if dedupe in reported:
            continue
        reported.add(dedupe)
        if status == "untracked":
            _fail(f"{doc_rel}:{lineno}: 反引号路径盘上存在但未入库: `{token}`"
                  f"（{rel}）——git add 之后其他人才能看到该文件", problems)
        else:
            _fail(f"{doc_rel}:{lineno}: 反引号路径在 git 视角下不存在: `{token}`"
                  f"（已入库 ∪ 已忽略 ∪ 历史并集均无 {rel}）", problems)

    for skip in skipped:
        print(f"SKIP: {skip}")
    if problems:
        for problem in problems:
            print(f"ERROR: {problem}", file=sys.stderr)
        print(f"FAIL: 反引号路径门禁发现 {len(problems)} 处问题", file=sys.stderr)
        return 1
    print(f"OK: {len(tokens)} 个反引号路径 token 全部可达"
          f"（git 视角；{len(skipped)} 处判为非仓库锚定并跳过）")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))

#!/usr/bin/env python3
"""Unit tests for the repository's quality-gate scripts (pure stdlib).

These tests assert that the gates can actually fail — a gate that cannot
produce a nonzero exit code on broken input is not a gate. Each test drives
the real script as a subprocess (or imports it) against throwaway fixtures
under a temporary directory; the repository itself is never modified.

Run directly:
    python3 scripts/tests/test_gate_behaviors.py
Or via unittest discovery:
    python3 -m unittest discover -s scripts/tests
"""

import importlib.util
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

# scripts/tests/ -> scripts/ -> repo root
_REPO_ROOT = Path(__file__).resolve().parents[2]
_SCRIPTS_DIR = _REPO_ROOT / "scripts"
_GIT = shutil.which("git")


def _run_script(script: Path, *args: str,
                env: "dict[str, str] | None" = None) -> subprocess.CompletedProcess:
    """Run a gate script as a subprocess and capture its verdict.

    The scripts pin their streams to UTF-8, so decode as UTF-8 as well: the
    parent locale is not a safe guess (it would turn Chinese verdicts into
    mojibake on any host whose console code page is not UTF-8). ``env`` adds
    overrides on top of the current environment.
    """
    child_env = None if env is None else {**os.environ, **env}
    return subprocess.run(
        [sys.executable, str(script), *args],
        capture_output=True,
        encoding="utf-8",
        errors="replace",
        env=child_env,
        timeout=120,
    )


def _load_module(script: Path, module_name: str):
    spec = importlib.util.spec_from_file_location(module_name, script)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def _repo_is_full_git_clone() -> bool:
    """True when _REPO_ROOT is a git worktree with complete history.

    Only then do the git-backed gates have enough evidence to certify, so
    only then is a "must pass" assertion about them meaningful. Wizard-
    generated projects are not git repositories at creation time and run
    this same test file, so the positive test must skip there.
    """
    if not _GIT:
        return False
    inside = subprocess.run(
        [_GIT, "rev-parse", "--is-inside-work-tree"],
        cwd=_REPO_ROOT, capture_output=True, text=True)
    if inside.returncode != 0 or inside.stdout.strip() != "true":
        return False
    shallow = subprocess.run(
        [_GIT, "rev-parse", "--is-shallow-repository"],
        cwd=_REPO_ROOT, capture_output=True, text=True)
    return not (shallow.returncode == 0 and shallow.stdout.strip() == "true")


class TestDeclDefGate(unittest.TestCase):
    """scripts/check_decl_def.py must pass clean trees and fail broken ones."""

    def setUp(self) -> None:
        self.script = _SCRIPTS_DIR / "check_decl_def.py"
        self.assertTrue(self.script.is_file(), f"missing gate script: {self.script}")
        self.tmp = Path(tempfile.mkdtemp(prefix="gate-test-decldef-"))
        self.addCleanup(shutil.rmtree, self.tmp, ignore_errors=True)

    def test_clean_repo_passes(self) -> None:
        result = _run_script(self.script, str(_REPO_ROOT))
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("OK:", result.stdout)

    def test_empty_scan_surface_fails(self) -> None:
        """A gate that scans nothing must not report OK (scan-surface floor)."""
        empty = self.tmp / "empty"
        empty.mkdir()
        result = _run_script(self.script, str(empty))
        self.assertEqual(result.returncode, 1)
        self.assertIn("解析失败", result.stderr)

    def test_explicit_floor_is_enforced(self) -> None:
        """An explicit floor above the real scan surface must fail.

        This is what the template-strict registration (inside the wizard
        markers) relies on; it must hold for any tree, not just this repo.
        """
        result = _run_script(self.script, str(_REPO_ROOT),
                             "--min-declarations", "100000")
        self.assertEqual(result.returncode, 1)
        self.assertIn("下限 100000", result.stderr)

    def test_floor_at_or_below_surface_passes(self) -> None:
        result = _run_script(self.script, str(_REPO_ROOT),
                             "--min-declarations", "1",
                             "--min-definitions", "1")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_missing_definition_fails_and_names_location(self) -> None:
        """Removing one definition from a copy of the tree must fail, naming
        the header and the function."""
        work = self.tmp / "repo"
        shutil.copytree(_REPO_ROOT / "include", work / "include")
        shutil.copytree(_REPO_ROOT / "src", work / "src")

        # Drop the definition body of the public status-string function in
        # src/wechatvoiceexport/error.c (implementation of the error public
        # header).
        target = work / "src" / "wechatvoiceexport" / "error.c"
        self.assertTrue(target.is_file(), "fixture expects src/wechatvoiceexport/error.c")
        text = target.read_text(encoding="utf-8")
        # Find the first public function definition (same shape the gate
        # matches: `name(...) {`) and delete the whole function, from the
        # start of its line (return type) to its closing brace.
        import re
        match = re.search(r"\b([A-Za-z_][A-Za-z0-9_]*)\s*\([^;{}]*\)\s*\{",
                          text)
        assert match is not None, "fixture expects a top-level definition"
        start = text.rfind("\n", 0, match.start()) + 1
        depth = 0
        end = start
        for i in range(match.end() - 1, len(text)):
            if text[i] == "{":
                depth += 1
            elif text[i] == "}":
                depth -= 1
                if depth == 0:
                    end = i + 1
                    break
        patched = text[:start] + text[end:]
        target.write_text(patched, encoding="utf-8")

        result = _run_script(self.script, str(work))
        self.assertEqual(result.returncode, 1)
        self.assertIn("public declaration without definition", result.stderr)


class TestVersionConsistencyGate(unittest.TestCase):
    """scripts/check_version_consistency.py parse helpers must be exact."""

    def setUp(self) -> None:
        script = _SCRIPTS_DIR / "check_version_consistency.py"
        self.assertTrue(script.is_file(), f"missing gate script: {script}")
        self.module = _load_module(script, "check_version_consistency")
        self.tmp = Path(tempfile.mkdtemp(prefix="gate-test-version-"))
        self.addCleanup(shutil.rmtree, self.tmp, ignore_errors=True)

    def test_parses_project_version(self) -> None:
        cmakelists = self.tmp / "CMakeLists.txt"
        cmakelists.write_text(
            "cmake_minimum_required(VERSION 3.21)\n"
            "project(SomeName\n    VERSION 1.2.3\n    LANGUAGES C\n)\n",
            encoding="utf-8",
        )
        parsed = self.module.read_cmake_version(cmakelists)
        self.assertEqual(parsed, ("SomeName", "1.2.3"))

    def test_rejects_missing_project_version(self) -> None:
        cmakelists = self.tmp / "CMakeLists.txt"
        cmakelists.write_text("cmake_minimum_required(VERSION 3.21)\n",
                              encoding="utf-8")
        self.assertIsNone(self.module.read_cmake_version(cmakelists))

    def test_parses_newest_changelog_section(self) -> None:
        changelog = self.tmp / "CHANGELOG.md"
        changelog.write_text(
            "# 更新日志\n\n## 0.3.0 — 2026-01-01\n\n- x\n\n## 0.2.0\n\n- y\n",
            encoding="utf-8",
        )
        self.assertEqual(self.module.read_changelog_version(changelog), "0.3.0")

    def test_reads_generated_header(self) -> None:
        header = self.tmp / "version.h"
        header.write_text(
            '#define SOMEPROJ_VERSION_STRING "4.5.6"\n', encoding="utf-8")
        self.assertEqual(self.module.read_header_version(header, "SomeProj"),
                         "4.5.6")

    def test_missing_inputs_exit_nonzero(self) -> None:
        """Pointing the gate at an empty tree must fail, not pass vacuously."""
        empty = self.tmp / "empty"
        empty.mkdir()
        result = _run_script(
            _SCRIPTS_DIR / "check_version_consistency.py",
            str(empty),
            "--build-dir", str(empty),
            "--exe", str(empty / "no-such-exe"),
        )
        self.assertEqual(result.returncode, 1)


class TestGitEvidenceGates(unittest.TestCase):
    """Gates that judge against git must refuse to certify without evidence.

    Two failure modes were observed in CI and must stay fixed:
    - a non-git directory (no evidence at all), and
    - a shallow clone (history/tags truncated, so a "path does not exist" or
      "no tags" verdict would be a false result rather than a finding).
    """

    def setUp(self) -> None:
        self.doc_paths = _SCRIPTS_DIR / "check_doc_paths.py"
        self.changelog_tag = _SCRIPTS_DIR / "check_changelog_tag.py"
        for script in (self.doc_paths, self.changelog_tag):
            self.assertTrue(script.is_file(), f"missing gate script: {script}")
        self.tmp = Path(tempfile.mkdtemp(prefix="gate-test-gitevidence-"))
        self.addCleanup(shutil.rmtree, self.tmp, ignore_errors=True)

    def test_non_git_directory_reports_cannot_verify(self) -> None:
        plain = self.tmp / "plain"
        plain.mkdir()
        (plain / "CHANGELOG.md").write_text(
            "# 更新日志\n\n## 1.0.0 — 2026-01-01\n\n- x\n", encoding="utf-8")
        for script in (self.doc_paths, self.changelog_tag):
            result = _run_script(script, str(plain))
            self.assertEqual(result.returncode, 1,
                             f"{script.name}: {result.stdout}{result.stderr}")
            self.assertIn("无法取证", result.stderr)

    @unittest.skipUnless(_GIT, "git not available")
    def test_shallow_clone_reports_cannot_verify(self) -> None:
        git_exec = _GIT
        assert git_exec is not None
        origin = self.tmp / "origin"
        origin.mkdir()
        (origin / "CHANGELOG.md").write_text(
            "# 更新日志\n\n## 1.0.0 — 2026-01-01\n\n- x\n", encoding="utf-8")
        (origin / "README.md").write_text("# doc\n", encoding="utf-8")

        def git(*args: str, cwd: Path) -> None:
            subprocess.run([git_exec, *args], cwd=cwd, check=True,
                           capture_output=True)

        git("init", "-q", ".", cwd=origin)
        git("config", "user.email", "probe@local", cwd=origin)
        git("config", "user.name", "probe", cwd=origin)
        git("-c", "commit.gpgsign=false", "add", "-A", cwd=origin)
        git("-c", "commit.gpgsign=false", "commit", "-q", "-m", "init",
            cwd=origin)
        git("-c", "tag.gpgsign=false", "tag", "v1.0.0", cwd=origin)

        shallow = self.tmp / "shallow"
        subprocess.run(
            [git_exec, "clone", "-q", "--depth", "1",
             origin.as_uri(), str(shallow)],
            check=True, capture_output=True)

        # Sanity: the clone really is shallow and really lost the tag.
        probe = subprocess.run(
            [git_exec, "rev-parse", "--is-shallow-repository"],
            cwd=shallow, capture_output=True, text=True, check=True)
        self.assertEqual(probe.stdout.strip(), "true")

        for script in (self.doc_paths, self.changelog_tag):
            result = _run_script(script, str(shallow))
            self.assertEqual(result.returncode, 1,
                             f"{script.name}: {result.stdout}{result.stderr}")
            self.assertIn("无法取证", result.stderr)

    @unittest.skipUnless(_repo_is_full_git_clone(),
                         "repository is not a full-history git clone")
    def test_clean_repo_verdicts_pass(self) -> None:
        """In this (full-history) repo both gates must certify normally."""
        for script in (self.doc_paths, self.changelog_tag):
            result = _run_script(script, str(_REPO_ROOT))
            self.assertEqual(result.returncode, 0,
                             f"{script.name}: {result.stdout}{result.stderr}")


class TestLegacyConsoleEncoding(unittest.TestCase):
    """Gate verdicts must stay readable on a non-UTF-8 console.

    Windows runners default to cp1252 (Chinese Windows desktops to GBK).
    Before the scripts pinned their streams to UTF-8, a Chinese verdict sent
    to stdout raised UnicodeEncodeError (the gate crashed) and the same
    verdict sent to stderr degraded to literal escape sequences (the output
    became unreadable). PYTHONIOENCODING recreates that console here, so the
    regression is caught on any platform.
    """

    _LEGACY_CONSOLE = {"PYTHONIOENCODING": "cp1252"}

    def setUp(self) -> None:
        self.tmp = Path(tempfile.mkdtemp(prefix="gate-test-console-"))
        self.addCleanup(shutil.rmtree, self.tmp, ignore_errors=True)
        self.empty = self.tmp / "empty"
        self.empty.mkdir()

    def test_stdout_verdicts_survive_legacy_console(self) -> None:
        """A passing gate prints its Chinese verdict to stdout."""
        tree = self.tmp / "docs-tree"
        tree.mkdir()
        (tree / "README.md").write_text("# doc\n", encoding="utf-8")

        result = _run_script(_SCRIPTS_DIR / "check_docs.py", str(tree),
                             "--mode", "links", env=self._LEGACY_CONSOLE)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("门禁通过", result.stdout)

    def test_stderr_verdicts_survive_legacy_console(self) -> None:
        """Every gate that prints a Chinese failure verdict to stderr."""
        # Non-git tree: the two git-evidence gates must refuse to certify.
        non_git = self.tmp / "non-git"
        non_git.mkdir()
        (non_git / "README.md").write_text("# doc\n", encoding="utf-8")
        (non_git / "CHANGELOG.md").write_text(
            "# 更新日志\n\n## 1.0.0 — 2026-01-01\n\n- x\n", encoding="utf-8")

        version_tree = self.tmp / "version-tree"
        version_tree.mkdir()
        (version_tree / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.21)\n"
            "project(Probe\n    VERSION 1.2.3\n    LANGUAGES C\n)\n",
            encoding="utf-8")

        cases = (
            ("check_decl_def.py", (str(self.empty),), "解析失败"),
            ("check_doc_paths.py", (str(non_git),), "无法取证"),
            ("check_changelog_tag.py", (str(non_git),), "无法取证"),
            ("check_version_consistency.py",
             (str(version_tree),
              "--build-dir", str(self.tmp / "missing-build"),
              "--exe", str(self.tmp / "missing-exe")), "构建目录不存在"),
            ("coverage_ratchet.py",
             (str(self.tmp / "missing-build"), "--repo-root", str(self.empty)),
             "构建目录不存在"),
        )
        for name, args, expected in cases:
            with self.subTest(script=name):
                result = _run_script(_SCRIPTS_DIR / name, *args,
                                     env=self._LEGACY_CONSOLE)
                self.assertEqual(result.returncode, 1,
                                 result.stdout + result.stderr)
                self.assertIn(expected, result.stderr, result.stderr)


class TestDocGatesScanSurface(unittest.TestCase):
    """The doc gates judge the repository, not the working directory.

    A locally ignored scratch directory (``.hermes/``, ``dist/``) used to be
    scanned like any other: a plan file referencing paths that never existed
    in the repository turned both doc gates red on a developer machine while
    CI - which has no such directory - stayed green. The scan surface is now
    git's live set (tracked ∪ untracked-not-ignored). These tests pin that
    down in both directions: ignored files must not flip a verdict, and live
    files (tracked *or* untracked) must still be judged.
    """

    _MIN_TOKENS = 10  # scan-surface floor inside check_doc_paths.py

    def setUp(self) -> None:
        self.script_docs = _SCRIPTS_DIR / "check_docs.py"
        self.script_paths = _SCRIPTS_DIR / "check_doc_paths.py"
        for script in (self.script_docs, self.script_paths):
            self.assertTrue(script.is_file(), f"missing gate script: {script}")
        self.tmp = Path(tempfile.mkdtemp(prefix="gate-test-surface-"))
        self.addCleanup(shutil.rmtree, self.tmp, ignore_errors=True)

        self.repo = self.tmp / "repo"
        (self.repo / "docs").mkdir(parents=True)
        (self.repo / ".gitignore").write_text(".scratch/\n", encoding="utf-8")
        (self.repo / "README.md").write_text("# repo\n", encoding="utf-8")
        (self.repo / "docs" / "a.md").write_text("# a\n", encoding="utf-8")

        # Enough distinct, existing backtick tokens to clear the gate's
        # scan-surface floor once the ignored junk is out of the surface.
        token_lines = ["`.scratch/junk.md` is scratch"]
        for index in range(self._MIN_TOKENS + 2):
            name = f"docs/f{index:02d}.md"
            (self.repo / name).write_text("# filler\n", encoding="utf-8")
            token_lines.append(f"see `{name}`")
        self.live_doc_text = "\n".join(token_lines) + "\n"
        (self.repo / "docs" / "a.md").write_text(self.live_doc_text,
                                                 encoding="utf-8")

        # Ignored scratch: broken relative link + a token that resolves
        # nowhere. Neither may reach the gates' verdicts.
        scratch = self.repo / ".scratch"
        scratch.mkdir()
        (scratch / "junk.md").write_text(
            "# junk\n\n[dead](no-such-target.md)\n\nsee `no/such/path`\n",
            encoding="utf-8")

        self.git = _GIT
        if self.git is None:
            self.skipTest("git not available")
        for args in (("init", "-q", "."),
                     ("config", "user.email", "probe@local"),
                     ("config", "user.name", "probe")):
            subprocess.run([self.git, *args], cwd=self.repo, check=True,
                           capture_output=True)
        subprocess.run(
            [self.git, "-c", "commit.gpgsign=false", "add", "-A"],
            cwd=self.repo, check=True, capture_output=True)
        subprocess.run(
            [self.git, "-c", "commit.gpgsign=false", "commit", "-q", "-m",
             "init"],
            cwd=self.repo, check=True, capture_output=True)

    def _scratch_ignored(self) -> None:
        result = subprocess.run(
            [self.git, "check-ignore", "-q", ".scratch/junk.md"], cwd=self.repo,
            capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, "fixture: .scratch/ must be ignored")

    def test_ignored_docs_do_not_flip_verdicts(self) -> None:
        self._scratch_ignored()
        for script, args in ((self.script_docs, ("--mode", "links")),
                             (self.script_paths, ())):
            with self.subTest(script=script.name):
                result = _run_script(script, str(self.repo), *args)
                self.assertEqual(result.returncode, 0,
                                 result.stdout + result.stderr)

    def test_live_tracked_doc_is_still_judged(self) -> None:
        self._scratch_ignored()
        # ``docs/`` exists, so this token is judged (it is not skipped as an
        # install/build-tree layout reference).
        broken = self.live_doc_text + "\nsee `docs/gone.md`\n"
        (self.repo / "docs" / "a.md").write_text(broken, encoding="utf-8")

        result = _run_script(self.script_paths, str(self.repo))
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        # The gate reports repo-relative paths with the platform separator.
        reported = result.stderr.replace("\\", "/")
        self.assertIn("docs/a.md", reported)
        self.assertIn("docs/gone.md", reported)

    def test_live_untracked_doc_is_still_judged(self) -> None:
        """Untracked-but-not-ignored files are part of the surface too."""
        self._scratch_ignored()
        (self.repo / "docs" / "untracked.md").write_text(
            "# untracked\n\n[dead](no-such-target.md)\n", encoding="utf-8")

        result = _run_script(self.script_docs, str(self.repo), "--mode", "links")
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn("untracked.md", result.stderr)
        self.assertIn("no-such-target.md", result.stderr)


if __name__ == "__main__":
    unittest.main(verbosity=2)

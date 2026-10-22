#!/usr/bin/env bash
# Static analysis entry point — the SINGLE implementation shared by CI
# (.github/workflows/static-analysis.yml) and local scripts/check.sh.
#
# Subcommands:
#   format-check   clang-format --dry-run --Werror over include/ src/ tests/
#                  (tests/vendor excluded); --fix rewrites in place instead.
#   tidy           clang-tidy --warnings-as-errors='*' over src/ and tests/
#                  (vendor excluded), using the compile database when available.
#   cppcheck       cppcheck --error-exitcode=1 over src/ and tests/
#                  (tests/vendor excluded).
#
# Options:
#   --build-dir <dir>   compile-database location (default: build/ninja-debug)
#   --require-tools     missing tool = hard failure (CI semantics). Without
#                       it, a missing tool prints a skip note and exits 0
#                       (local convenience semantics, same as check.sh has
#                       always had).
#
# Only *.c/*.h/*.cpp are ever passed to clang-format — never .py or other
# file types.
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
cd "$REPO_ROOT"

FILE_LIST=".static-analysis-files.txt"
trap 'rm -f "$FILE_LIST"' EXIT

BUILD_DIR="build/ninja-debug"
REQUIRE_TOOLS=0
FIX=0
MODE=""

show_usage() {
  echo "Usage: $0 {format-check|tidy|cppcheck} [--build-dir <dir>] [--require-tools] [--fix]"
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    format-check|tidy|cppcheck)
      if [ -n "$MODE" ]; then echo "ERROR: only one subcommand allowed" >&2; exit 2; fi
      MODE="$1"; shift ;;
    --build-dir)
      [ "$#" -ge 2 ] || { echo "ERROR: --build-dir requires a value" >&2; exit 2; }
      BUILD_DIR="$2"; shift 2 ;;
    --require-tools) REQUIRE_TOOLS=1; shift ;;
    --fix) FIX=1; shift ;;
    -h|--help) show_usage; exit 0 ;;
    *) echo "ERROR: unknown argument: $1" >&2; show_usage >&2; exit 2 ;;
  esac
done

if [ -z "$MODE" ]; then show_usage >&2; exit 2; fi

tool_missing() {
  # $1 = tool name
  if [ "$REQUIRE_TOOLS" -ne 0 ]; then
    echo "ERROR: required tool not found: $1" >&2
    exit 1
  fi
  echo "Skipping $1: command not found."
  exit 0
}

case "$MODE" in
  format-check)
    command -v clang-format >/dev/null 2>&1 || tool_missing clang-format
    # File list: C/C++ sources and headers, vendored code excluded.
    find include src tests \( -path tests/vendor \) -prune -o \
      -type f \( -name '*.c' -o -name '*.h' -o -name '*.cpp' \) -print \
      | sort > .static-analysis-files.txt
    if [ ! -s .static-analysis-files.txt ]; then
      echo "ERROR: format scan surface is empty (no .c/.h/.cpp found)" >&2
      rm -f .static-analysis-files.txt
      exit 1
    fi
    if [ "$FIX" -ne 0 ]; then
      xargs clang-format -i < .static-analysis-files.txt
    else
      xargs clang-format --dry-run --Werror < .static-analysis-files.txt
    fi
    rm -f .static-analysis-files.txt
    ;;
  tidy)
    command -v clang-tidy >/dev/null 2>&1 || tool_missing clang-tidy
    find src tests \
      \( -path tests/vendor \) \
      -prune -o -type f -name '*.c' -print \
      | sort > .static-analysis-files.txt
    if [ ! -s .static-analysis-files.txt ]; then
      echo "ERROR: clang-tidy scan surface is empty (no .c found)" >&2
      rm -f .static-analysis-files.txt
      exit 1
    fi
    GENERATED_INCLUDE_DIR="$BUILD_DIR/generated/include"
    if [ -f "$BUILD_DIR/compile_commands.json" ]; then
      xargs clang-tidy --warnings-as-errors='*' -p "$BUILD_DIR" < .static-analysis-files.txt
    else
      # No compile database: run per file with explicit compile flags after
      # `--` (same semantics as the previous check.sh implementation).
      xargs -I{} clang-tidy --warnings-as-errors='*' --extra-arg=-std=c11 \
        {} -- -Iinclude -I"$GENERATED_INCLUDE_DIR" < .static-analysis-files.txt
    fi
    rm -f .static-analysis-files.txt
    ;;
  cppcheck)
    command -v cppcheck >/dev/null 2>&1 || tool_missing cppcheck
    find src tests \( -path tests/vendor \) -prune -o -type f -name '*.c' -print \
      | sort > .static-analysis-files.txt
    if [ ! -s .static-analysis-files.txt ]; then
      echo "ERROR: cppcheck scan surface is empty (no .c found)" >&2
      rm -f .static-analysis-files.txt
      exit 1
    fi
    cppcheck \
      --std=c11 \
      --enable=warning,performance,portability \
      --max-configs=1 \
      --inline-suppr \
      --quiet \
      --error-exitcode=1 \
      --file-list=.static-analysis-files.txt \
      -I include \
      -I "$BUILD_DIR/generated/include"
    rm -f .static-analysis-files.txt
    ;;
esac

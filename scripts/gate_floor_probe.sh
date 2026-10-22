#!/usr/bin/env bash
# Gate floor probe: every scripts/check_*.py quality gate must FAIL (nonzero
# exit) with a naming diagnostic as its first output line when its scan
# surface is empty. A gate that prints OK on an empty scan is a defect.
#
# Usage: ./scripts/gate_floor_probe.sh
# Exit: 0 = every probed gate failed as required; 1 = at least one gate
#       passed vacuously or probed inconclusively.
#
# Notes:
# - Probe targets are discovered by glob (scripts/check_*.py), so new gates
#   are covered automatically.
# - Gates with extra required arguments get a per-gate case entry below; an
#   unknown gate is probed with the plain "<empty dir>" convention and any
#   usage-error exit (2) is reported as INCONCLUSIVE, not as a pass.
set -u

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)

PYTHON=${PYTHON:-python3}
EMPTY_DIR=$(mktemp -d "${TMPDIR:-/tmp}/gate-floor-probe-XXXXXX")
trap 'rm -rf "$EMPTY_DIR"' EXIT

overall=0
probed=0

printf '%-34s %-6s %s\n' "GATE" "EXIT" "FIRST LINE"
for gate in "$REPO_ROOT"/scripts/check_*.py; do
  [ -f "$gate" ] || continue
  name=$(basename "$gate")
  case "$name" in
    check_version_consistency.py)
      out=$("$PYTHON" "$gate" "$EMPTY_DIR" \
        --build-dir "$EMPTY_DIR" --exe "$EMPTY_DIR/no-such-exe" 2>&1)
      rc=$?
      ;;
    *)
      out=$("$PYTHON" "$gate" "$EMPTY_DIR" 2>&1)
      rc=$?
      ;;
  esac
  first_line=$(printf '%s\n' "$out" | head -n 1)
  probed=$((probed + 1))

  verdict="FAIL-AS-REQUIRED"
  if [ "$rc" -eq 0 ]; then
    verdict="VACUOUS-PASS"
    overall=1
  elif [ "$rc" -eq 2 ]; then
    verdict="INCONCLUSIVE (usage error; add a probe case for this gate)"
    overall=1
  elif ! printf '%s' "$first_line" | grep -qE 'FAIL|ERROR|解析失败'; then
    verdict="NO-NAMED-DIAGNOSTIC"
    overall=1
  fi

  printf '%-34s %-6s %s\n' "$name" "$rc" "$first_line"
  printf '%-34s %-6s -> %s\n' "" "" "$verdict"
done

if [ "$probed" -eq 0 ]; then
  echo "FAIL: no scripts/check_*.py gates found to probe" >&2
  exit 1
fi

echo ""
if [ "$overall" -eq 0 ]; then
  echo "OK: all $probed probed gates fail on an empty scan surface"
else
  echo "FAIL: at least one gate does not fail properly on an empty scan surface" >&2
fi
exit "$overall"

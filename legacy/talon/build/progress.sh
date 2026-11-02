#!/usr/bin/env bash
# The scoreboard: how much of the build is byte-identical to the shipped
# tools, held to what has already been recorded (progress/).
#
#   make progress           (on the host: `build/progress.sh check` in a
#                            container; COMPLETE=1, COVERED=1)
#   make progress-update    (`build/progress.sh update`; ACCEPT_LOSSES=1)
#
#   check   tools/progress.py, then tools/ratchet.py check: fails if a
#           function recorded in progress/identical.txt no longer matches
#           (COMPLETE=1: also unless every function matches and nothing
#           is unwritten or unplaced; COVERED=1: the latter alone)
#   update  tools/progress.py, then tools/ratchet.py update
#   both    then tools/datacheck.py (the text in our images' data against
#           the shipped images'; progress/data-known.txt) and
#           tools/fpconst.py --targets (the float constants behind masked
#           relocations) and tools/vtabcheck.py --targets (vtables, and the
#           callees and globals placed functions reach;
#           progress/vtab-known.txt): each fails the run on a difference
#           not known
#
# Before anything is scored, out/reference/ is checked against
# progress/reference.sha256, the release files as pinned: a stale or
# altered cache would be scored as if it were the release.
set -e
cd "$(dirname "${BASH_SOURCE[0]}")/.."

mode="${1:-check}"
case "$mode" in
    check|update) ;;
    *) echo "usage: build/progress.sh check|update" >&2; exit 2 ;;
esac

if ! (cd out/reference 2>/dev/null && sha256sum --quiet -c ../../progress/reference.sha256) 2>/dev/null; then
    echo "== out/reference/ is missing or not the release files progress/reference.sha256 names: extracting"
    bash build/reference.sh
fi

python3 tools/progress.py
echo
rc=0
if [ "$mode" = check ]; then
    python3 tools/ratchet.py check ${COMPLETE:+--complete} ${COVERED:+--covered} || rc=$?
else
    python3 tools/ratchet.py update ${ACCEPT_LOSSES:+--accept-losses} || rc=$?
fi
echo
python3 tools/datacheck.py "$mode" || rc=1
echo
echo "float constants (tools/fpconst.py):"
python3 tools/fpconst.py --targets || rc=1
# ... and every vtable slot and every call and global our identically placed
# functions reach, against the shipped ones (progress/vtab-known.txt)
echo
echo "vtables and wiring (tools/vtabcheck.py):"
python3 tools/vtabcheck.py --targets || rc=1
# ... and what all of it rested on (the toolchains' digests checked), kept
# with the baseline when one is recorded
echo
python3 tools/evidence.py || rc=1
[ "$mode" = update ] && cp out/progress/evidence.json progress/evidence.json
exit $rc

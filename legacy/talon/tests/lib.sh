# Shared by the test suites (sourced; run inside the build container, from
# the talon directory, as tests/run.sh does).
#
# Every suite runs the shipped tool and ours on the same inputs, in the same
# conditions, and compares what they write: the shipped tool is the
# reference, so no expected output is stored here and nothing depends on the
# host's Wine behaving like another's.

TALON=${TALON:-$PWD}
export TOOLS_PROJECT="$TALON"
REFERENCE="$TALON/out/reference"
WORK="$TALON/out/tests"
# outside out/tests: its dosdevices/z: is the whole filesystem, which must
# not be in what CI keeps of a failed run
export WINEPREFIX="$TALON/out/tests-wine" WINEDEBUG=-all
TEST_DISPLAY=:97
FAILED=0

log() { printf '%s\n' "$*"; }
pass() { log "  PASS  $*"; }
fail() { log "  FAIL  $*"; FAILED=$((FAILED + 1)); }

need() {   # need <file> <what to run to make it>
    [ -e "$1" ] || { log "missing $1: run $2 first"; exit 2; }
}

# One Wine prefix and one virtual display for the whole run.
# Both are waited for, not slept on: a fresh prefix (always, in CI) goes on
# setting itself up after wineboot returns, and a busy runner can take more
# than a second to bring Xvfb up; either made the first tool started exit
# without doing anything.
start_wine() {
    mkdir -p "$WORK"
    if [ ! -d "$WINEPREFIX/drive_c" ]; then
        wineboot -i >/dev/null 2>&1
        wineserver -w
    fi
    Xvfb "$TEST_DISPLAY" -screen 0 1024x768x24 >/dev/null 2>&1 &
    XVFB_PID=$!
    trap stop_wine EXIT
    local i sock="/tmp/.X11-unix/X${TEST_DISPLAY#:}"
    for ((i = 0; i < 100; i++)); do
        [ -S "$sock" ] && return 0
        sleep 0.1
    done
    log "Xvfb $TEST_DISPLAY did not come up"
    exit 2
}
stop_wine() {
    wineserver -k >/dev/null 2>&1 || true
    [ -n "${XVFB_PID:-}" ] && kill "$XVFB_PID" 2>/dev/null || true
}

# run_tool <timeout s> <working dir> <exe> [args...]: run a Windows tool in
# <working dir> until it exits by itself; 124 if it had to be killed.
run_tool() {
    local limit=$1 dir=$2 rc=0
    shift 2
    (cd "$dir" && DISPLAY=$TEST_DISPLAY timeout "$limit" wine "$@" >wine.log 2>&1) || rc=$?
    wineserver -k >/dev/null 2>&1 || true
    return $rc
}

# run_tool_until <timeout s> <working dir> <file> <regex> <exe> [args...]:
# run a Windows tool that never exits by itself (its window stays up when it
# is done) until <file>, in <working dir>, has a line matching <regex>, then
# stop it; 124 if that never came.
run_tool_until() {
    local limit=$1 dir=$2 file=$3 re=$4 pid i
    shift 4
    (cd "$dir" && DISPLAY=$TEST_DISPLAY exec wine "$@" >wine.log 2>&1) &
    pid=$!
    for ((i = 0; i < limit; i++)); do
        grep -qs -- "$re" "$dir/$file" && break
        kill -0 $pid 2>/dev/null || break
        sleep 1
    done
    sleep 1   # the log line comes after the .dat is closed; let it settle
    grep -qs -- "$re" "$dir/$file"
    local rc=$?
    wineserver -k >/dev/null 2>&1 || true
    wait $pid 2>/dev/null
    [ $rc -eq 0 ] || return 124
}

# wine_tail <dir>: the end of the wine.log a run left in <dir>, for a failure
wine_tail() { tail -n 5 "$1/wine.log" 2>/dev/null | sed 's/^/        /'; }

# winpath <path>: the Z: path Wine gives a Unix path
winpath() { printf 'Z:%s' "$(realpath -m "$1" | tr / '\\')"; }

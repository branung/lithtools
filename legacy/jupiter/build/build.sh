#!/bin/bash
# Build the Jupiter tools in the build container.
#
#   legacy/jupiter/build/build.sh [vcbuild.py options] [project ...]
#   legacy/jupiter/build/build.sh --shell          (a shell in the container)
#
# Runs legacy/jupiter/build/vcbuild.py on legacy/jupiter/src/Engine/tools/Tools.sln in the
# cjacksrc build image (Ubuntu 24.04 with 32-bit Wine), as the invoking user,
# with this repository at /repo and the VC .NET 2003 tree at /tc.
#
# Settings (environment):
#   IMAGE      build image (default: cjacksrc-build)
#   VC71       host path of the msvc-7.10-3077 tree
#              (default: the repository's ext/toolchains/msvc-7.10-3077)
#   B69        Build 69 unpacked by file group (default: extracted/jupiter-b69/x)
#   DXSDK      DirectX SDK, February 2006: Include/ and Lib/x86/
#              (default: extracted/dxsdk-feb2006)
#
# Before building, the plug-in SDKs (Engine/tools/Plugins/SDKFiles) are
# copied in from Build 69; they are gitignored. DEdit is built in its
# "Release Public Development" configuration, as Monolith's public builds
# were (its default game executable is LithTechDev.exe). The 3ds Max 7 and
# Maya 6.5/7.0 plug-ins are built; the others' SDKs are not in Build 69.
set -euo pipefail
REPO="$(cd "$(dirname "$0")/../../.." && pwd)"
IMAGE="${IMAGE:-cjacksrc-build}"
VC71="$(realpath "${VC71:-$REPO/ext/toolchains/msvc-7.10-3077}")"
B69="$(realpath "${B69:-$REPO/extracted/jupiter-b69/x}")"
DXSDK="$(realpath "${DXSDK:-$REPO/extracted/dxsdk-feb2006}")"

for f in "$VC71/bin/cl.exe" "$DXSDK/Include/d3dx9.h" \
         "$B69/Engine/Engine/tools/Plugins/SDKFiles"; do
    [ -e "$f" ] || { echo "error: missing $f (see legacy/jupiter/README.md)" >&2; exit 1; }
done
case "$DXSDK" in "$REPO"/*) ;; *) echo "error: DXSDK must be inside the repository" >&2; exit 1;; esac

sdk="$REPO/jupiter/src/Engine/tools/Plugins/SDKFiles"
if [ ! -e "$sdk/.copied" ]; then
    echo "== copying the plug-in SDKs from Build 69"
    rm -rf "$sdk"
    cp -a "$B69/Engine/Engine/tools/Plugins/SDKFiles" "$sdk"
    touch "$sdk/.copied"
fi

tty=(); [ -t 0 ] && tty=(-it)
run=(docker run --rm --init "${tty[@]}" --user "$(id -u):$(id -g)"
     -v "$REPO:/repo" -v "$VC71:/tc/msvc-7.10-3077:ro"
     -e VC71=/tc/msvc-7.10-3077 -e DXSDK="/repo/${DXSDK#"$REPO"/}"
     -w /repo "$IMAGE")
if [ "${1:-}" = --shell ]; then
    exec "${run[@]}" bash
fi
# The plug-in projects for 3ds Max 3-6 and Maya 4.0-6.0 need SDKs Build 69
# does not carry; they are left out.
skip=()
for p in Max30 Max40 Max50 Max60 Maya40 Maya45 Maya50 Maya60; do
    skip+=(--skip "Plugins_$p")
done
# The Photoshop DTX plug-in has a solution of its own, outside Tools.sln; it
# is built after it, with the same options (but none of the project names).
opts=(); for a in "$@"; do case "$a" in --*) opts+=("$a");; esac; done
status=0
"${run[@]}" python3 legacy/jupiter/build/vcbuild.py \
    legacy/jupiter/src/Engine/tools/Tools.sln \
    --set "DEdit=Release Public Development" "${skip[@]}" "$@" || status=$?
"${run[@]}" python3 legacy/jupiter/build/vcbuild.py \
    legacy/jupiter/src/Engine/tools/Plugins/Photoshop/dtxformat/DTXFormat.sln "${opts[@]}" || status=$?
exit $status

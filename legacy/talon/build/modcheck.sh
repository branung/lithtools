#!/bin/bash
# Build one object of a target, match it against the shipped image, and list
# what is still unmatched on both sides within a range of the image.
#
#   build/modcheck.sh <target stem> <object stem> <lo> <hi>
#   build/modcheck.sh Processor-avp2 BasePoly 0x401000 0x4029f0
#
# The object is built through build/build.mk (in the build container); the
# target's image and function table come from
# progress/targets.py and out/reference/<stem>-functions.csv. Shipped rows
# of the range that no function of the object matches are listed, then the
# object's functions that match nothing (templates and initializers left out).
set -u
cd "$(dirname "$0")/.."
export TOOLS_PROJECT="$PWD"
tgt=$1 obj=$2 lo=$3 hi=$4
img=$(python3 -I -c "import sys; sys.path.insert(0,'progress'); import targets; print(next(t['shipped'] for t in targets.TARGETS if t['stem']=='$tgt'))")
dir=$(python3 -I -c "import sys,os; sys.path.insert(0,'progress'); import targets; print(os.path.dirname(next(t['built'] for t in targets.TARGETS if t['stem']=='$tgt')))")
O=$dir/$obj.obj
docker run --rm --init -u "$(id -u):$(id -g)" -e HOME=/tmp/home -e TOOLS_PROJECT=/repo \
    -v "$PWD:/repo" -v "$(cd ../../ext && pwd):/ext" -w /repo cjacksrc-build \
    make -s -f build/build.mk ${PFX:+PFX=$PFX} "$O" 2>&1 | grep -E "error" | head -20
[ -f "$O" ] || exit 1
out=$(python3 -I ../../ext/tools/exematch.py "$img" "$O" 2>&1)
echo "$out" | tail -1
matched=$(echo "$out" | grep -oE "MATCH 0x[0-9a-f]+" | awk '{print $2}' | sort -u)
echo "-- shipped rows in range not matched:"
awk -F, -v lo=$((lo)) -v hi=$((hi)) 'NR>1{a=strtonum($1); if (a>=lo && a<hi) print $1, $2}' \
    "out/reference/$tgt-functions.csv" | while read -r a s; do
    h=$(printf '0x%x' $((a))); grep -qx "$h" <<< "$matched" || echo "  $a $s"; done
echo "-- ours not matched:"
echo "$out" | grep -E "^\s+----" | grep -vE '_\$E|----     \?\?\$|----     \?[A-Za-z0-9_]+@\?\$'

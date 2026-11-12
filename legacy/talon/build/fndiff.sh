#!/bin/bash
# tools/fndiff.py on one shipped function of a target, its size looked up
# in the target's function table (or given).
#
#   build/fndiff.sh <target stem> <ship addr> <object stem> <symbol> [size]
set -u
cd "$(dirname "$0")/.."
export TOOLS_PROJECT="$PWD"
tgt=$1 a=$2 obj=$3 sym=$4
s=${5:-$(awk -F, -v a=$((a)) 'NR>1 && strtonum($1)==a {print $2}' "out/reference/$tgt-functions.csv")}
dir=$(python3 -I -c "import sys,os; sys.path.insert(0,'progress'); import targets; print(os.path.dirname(next(t['built'] for t in targets.TARGETS if t['stem']=='$tgt')))")
python3 -I ../../ext/tools/fndiff.py "$a" "$dir/$obj.obj" "$sym" --image="$tgt" --end="$(printf '0x%x' $((a + s)))"

#!/bin/bash
# Offsets and sizes of the Processor's classes as OUR headers lay them out,
# read back from the compiler: each query becomes `char (*x)[N] = 1;`, whose
# error message names N.
#
#   build/offsetof.sh 'CPreMainWorld::m_DefaultLMGridSize' 'sizeof(CNode)'
#
# A query is `Class::member` (offsetof) or any constant expression. The probe
# includes bdefs.h, preworld.h and bspgen.h and is compiled in
# src/LT2/tools/PreProcessor with the Processor's flags, in the build container.
set -u
cd "$(dirname "$0")/.."
dir=src/LT2/tools/PreProcessor
probe=_offsetof_probe_$$
{
    echo '#include "bdefs.h"'
    echo '#include "preworld.h"'
    echo '#include "bspgen.h"'
    echo '#include "lightmapmaker.h"'
    echo '#include <stddef.h>'
    i=0
    for q in "$@"; do
        if [[ $q == *::* && $q != *\(* ]]; then
            e="offsetof(${q%%::*}, ${q##*::})"
        else
            e="$q"
        fi
        echo "char (*q$i)[($e) + 1] = 1;"
        i=$((i + 1))
    done
} > "$dir/$probe.cpp"
docker run --rm --init -u "$(id -u):$(id -g)" -e HOME=/tmp/home -e TOOLS_PROJECT=/repo \
    -v "$PWD:/repo" -v "$(cd ../../ext && pwd):/ext" -w /repo cjacksrc-build \
    bash -c "cd $dir && WINEPREFIX=${PFX:-/repo/.wine/drives} WINEPATH='t:\\msvc-6.00-8966\\BIN' \
        INCLUDE='t:\\msvc-6.00-8966\\INCLUDE;t:\\msvc-6.00-8966\\MFC\\INCLUDE' \
        wine /ext/toolchains/msvc-6.00-8966/BIN/cl.exe /nologo /MT /GX /O2 /I . /I ..\\\\shared\\\\engine \
        /I ..\\\\shared\\\\world /I win /I ..\\\\DEdit\\\\draw /I ..\\\\..\\\\sdk\\\\inc \
        /I ..\\\\..\\\\lithshared\\\\stdlith /I ..\\\\..\\\\lithshared\\\\stdlith\\\\sys\\\\win \
        /I ..\\\\..\\\\lithshared\\\\regmgr /I ..\\\\..\\\\lithshared\\\\lith /I ..\\\\..\\\\lithshared\\\\incs \
        /I ..\\\\..\\\\lithshared\\\\stl /D WIN32 /D NDEBUG /D _WINDOWS /D PRE_FLOAT /D PREPROCESSOR_BUILD \
        /D BDEFS_MFC /D STDLITH_MFC /c /Fo$probe.obj $probe.cpp" 2>&1 \
    | python3 -c '
import re, sys
qs = sys.argv[1:]
for line in sys.stdin:
    m = re.search(r"\((\d+)\) : error C2440.*char \(\*\)\[(\d+)\]", line)
    if m:
        i = int(m.group(1)) - 6
        n = int(m.group(2)) - 1
        print("%-50s %#x (%d)" % (qs[i] if 0 <= i < len(qs) else "?", n, n))
    elif "error" in line and "C2440" not in line:
        print(line.rstrip())
' "$@"
rm -f "$dir/$probe.cpp" "$dir/$probe.obj"

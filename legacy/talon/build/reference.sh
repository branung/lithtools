#!/bin/bash
# Extract the shipped tool binaries into out/reference/, the ground truth the
# scoreboard and the reconstruction tools compare against. Nothing in the
# build reads it, and nothing in it is ever run.
#
#   build/reference.sh
#
# Inputs, each checked before use. By default rez/'s disc images, as
# rez/fetch-isos.sh avp2-ph-eu nolf-eu-goty leaves them in rez/isos/ (it
# checks the zips against rez/isos/SHA256SUMS); failing those, the raw
# images under ../../extracted/. Override in the environment:
#   AVP2_PH_ISO  the Aliens versus Predator 2: Primal Hunt (Europe) disc
#                (redump.org disc 35760) as a 2048-byte-sector .iso, or
#   AVP2_PH_BIN  the same disc's raw .bin (SHA-1 b47b0750...747d). Its Tools
#                folder has the AvP2 Editing Tools v1.0 (2001-12-14) and
#                Updates 1 and 2 (2002-01-31, 2002-05-06), InstallShield 6
#                self-extractors, each checked by SHA-256 whichever image
#                they come from.
#   NOLF_TOOLS   NOLFTools001.exe (SHA-256 b2a13a39...71d1: the NOLF Tools 001,
#                January 2001, a zip self-extractor), or
#   NOLF_DISC2   the NOLF Game of the Year disc 2 .iso (the rez/ releases
#                nolf-eu-goty and nolf-usa-goty) it is taken from.
#
# Writes out/reference/<release>/ for avp2tools, avp2tools_update,
# avp2tools_update2 and nolftools001, each the release's files as its
# installer lays them out, and SHA256SUMS over all of them.
set -euo pipefail
cd "$(dirname "$0")/.."
TALON="$PWD"
export TOOLS_PROJECT="$TALON"
ISOS="$TALON/../../ext/rez/isos"
PH_NAME="Aliens Versus Predator 2 - Primal Hunt (Europe)"
DISC2_NAME="Operative, The - No One Lives Forever (USA, Europe) (Disc 2) (Game of the Year Edition)"
if [ -z "${AVP2_PH_ISO:-}${AVP2_PH_BIN:-}" ]; then
    if [ -f "$ISOS/$PH_NAME.iso" ]; then AVP2_PH_ISO="$ISOS/$PH_NAME.iso"
    else AVP2_PH_BIN="$TALON/../../extracted/avp2-ph-eu/$PH_NAME.bin"; fi
fi
if [ -z "${NOLF_TOOLS:-}${NOLF_DISC2:-}" ]; then
    if [ -f "$ISOS/$DISC2_NAME.iso" ]; then NOLF_DISC2="$ISOS/$DISC2_NAME.iso"
    else NOLF_TOOLS="$TALON/../../extracted/nolf-tools-001/iso/Game/NOLFTools001.exe"; fi
fi
PH_SHA1=b47b07507ad6a511958b8544498d4b9399c2747d
NOLF_SHA256=b2a13a39a12642b8de66b541dea999c5315c6764f0b6897bcaf7a01e832071d1
declare -A TOOLS_SHA256=(
    [avp2tools]=828bc1c9e70b5fe1901e45c7b42b9948c4212990bc04910339057d72dcf4669e
    [avp2tools_update]=7c3fd1f389ed9833331ca6b2200f40f409ff64200c20ccf800c04a17707775ab
    [avp2tools_update2]=485a8ea0b8f5055ccbd40c1bdb2d2a100d54d0476d2c692955407b3398ba5662
)
REZ="$TALON/../../ext/rez/tools"
OUT="$TALON/out/reference"
TMP="$TALON/out/reference.tmp"

check() {   # check <file> <sha1|sha256> <digest>
    [ -f "$1" ] || { echo "error: missing $1 (see build/reference.sh)" >&2; exit 1; }
    local got; got=$("$2sum" "$1" | cut -d' ' -f1)
    [ "$got" = "$3" ] || { echo "error: $1: $2 $got, expected $3" >&2; exit 1; }
}
present() { [ -f "$1" ] || { echo "error: missing $1 (see build/reference.sh)" >&2; exit 1; }; }
if [ -n "${AVP2_PH_ISO:-}" ]; then present "$AVP2_PH_ISO"; else check "$AVP2_PH_BIN" sha1 "$PH_SHA1"; fi
if [ -n "${NOLF_DISC2:-}" ]; then present "$NOLF_DISC2"; else check "$NOLF_TOOLS" sha256 "$NOLF_SHA256"; fi

make -s -C "$REZ/unshield" >/dev/null
rm -rf "$TMP"; mkdir -p "$TMP"
if [ -n "${AVP2_PH_ISO:-}" ]; then
    ln -s "$AVP2_PH_ISO" "$TMP/ph.iso"
else

# The Primal Hunt disc: one Mode 1 (XA Form 1) data track of raw sectors; the
# ISO is each sector's 2048 bytes of user data.
python3 -I - "$AVP2_PH_BIN" "$TMP/ph.iso" <<'EOF'
import sys
src, dst = sys.argv[1:3]
with open(src, 'rb') as f, open(dst, 'wb') as o:
    while True:
        s = f.read(2352)
        if len(s) < 2352:
            break
        o.write(s[16:2064] if s[15] == 1 else s[24:2072])
EOF
fi
python3 -I "$REZ/isoextract.py" x "$TMP/ph.iso" "$TMP/ph" Tools >/dev/null
for n in "${!TOOLS_SHA256[@]}"; do check "$TMP/ph/Tools/$n.exe" sha256 "${TOOLS_SHA256[$n]}"; done
for n in avp2tools avp2tools_update avp2tools_update2; do
    python3 -I "$REZ/sfxcab.py" "$TMP/ph/Tools/$n.exe" "$TMP/sfx-$n" >/dev/null
    hdr=$(find "$TMP/sfx-$n" -iname data1.hdr | head -1)
    "$REZ/unshield/unshield" -d "$TMP/x-$n" x "$hdr" >/dev/null
    # the setup's own support groups (_Engine_*, _Support_*) are not the release
    mkdir -p "$TMP/$n"
    for g in "$TMP/x-$n"/*; do
        case "$(basename "$g")" in _*) ;; *) cp -a "$g"/. "$TMP/$n"/ ;; esac
    done
done

# NOLF Tools 001: a zip self-extractor around an InstallShield 5 setup whose
# files are stored plain in the zip.
if [ -n "${NOLF_DISC2:-}" ]; then
    python3 -I "$REZ/isoextract.py" x "$NOLF_DISC2" "$TMP/disc2" Game/NOLFTools001.exe >/dev/null
    NOLF_TOOLS="$TMP/disc2/Game/NOLFTools001.exe"
    check "$NOLF_TOOLS" sha256 "$NOLF_SHA256"
fi
python3 -I -c "import zipfile,sys; zipfile.ZipFile(sys.argv[1]).extractall(sys.argv[2])" \
    "$NOLF_TOOLS" "$TMP/nolftools001"

rm -rf "$OUT"; mkdir -p "$OUT"
for n in avp2tools avp2tools_update avp2tools_update2 nolftools001; do
    mv "$TMP/$n" "$OUT/$n"
done
rm -rf "$TMP"
# (written beside the tree and moved in, so it does not list itself; it
# must equal progress/reference.sha256, the release files as pinned)
(cd "$OUT" && find . -type f -print0 | LC_ALL=C sort -z | xargs -0 sha256sum) >"$OUT.SHA256SUMS"
mv "$OUT.SHA256SUMS" "$OUT/SHA256SUMS"
(cd "$OUT" && sha256sum --quiet -c SHA256SUMS)
if ! cmp -s "$OUT/SHA256SUMS" progress/reference.sha256; then
    echo "error: out/reference/ is not the release files progress/reference.sha256 names" >&2
    exit 1
fi
echo "out/reference: $(wc -l <"$OUT/SHA256SUMS") files, as progress/reference.sha256 pins them"

# Each target's function table (tools/enumfuncs.py), as progress/targets.py
# names them: out/reference/<stem>-functions.csv
python3 -I - <<'PY'
import os, subprocess, sys
sys.path.insert(0, 'progress')
import targets
for t in targets.TARGETS:
    if t.get('kind') != 'image':
        continue
    csv = os.path.join(targets.REFERENCE, t['stem'] + '-functions.csv')
    subprocess.run([sys.executable, '-I', '../../ext/tools/enumfuncs.py', t['shipped'], '--csv', csv],
                   check=True, stdout=subprocess.DEVNULL)
    print('wrote', csv)
PY

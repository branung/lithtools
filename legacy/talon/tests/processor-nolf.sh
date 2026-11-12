# Processor.exe (NOLF): every sample world of the NOLF Tools 001, processed
# by the shipped exe and by ours, with DEdit's default options and with
# lighting and full visibility, must give byte-identical .dat files.
#
# NOLF's processor differs from AvP2's in how it is run: it takes the world
# without its .ed (it adds that), and it takes no options on its command
# line, only from the first line of processor.opt in its working directory
# (DEdit writes it), -skipdialog included. It never exits by itself: the
# progress dialog stays up when it is done, so each run is stopped once the
# log (-logfile) has its last line. The worlds come from NOLFTOOLS001.REZ.

SHIPPED="$REFERENCE/nolftools001/Tools/Processor.exe"
OURS=${PROCESSOR_NOLF_OURS:-$TALON/src/LT2/built/Release/tools/ProcessorNOLF/Processor.exe}
REZ="$REFERENCE/nolftools001/Tools/NOLFTOOLS001.REZ"
need "$SHIPPED" "make reference"
need "$OURS" "make"

suite_processor_nolf() {
    local dir="$WORK/processor-nolf" w name side opts case
    rm -rf "$dir"
    mkdir -p "$dir/worlds"
    python3 -I "$TALON/../../ext/tools/rezextract.py" x "$REZ" "$dir/worlds" WORLDS/ >/dev/null
    for side in ship ours; do
        mkdir -p "$dir/$side/bin"
    done
    cp "$SHIPPED" "$dir/ship/bin/Processor.exe"
    cp "$OURS" "$dir/ours/bin/Processor.exe"

    for case in default full; do
        case $case in
            default) opts="-bsp -FixTJunc" ;;                  # DEdit's dialog as it opens
            full)    opts="-bsp -light -optimize -FixTJunc" ;;  # with lighting and full visibility
        esac
        for w in SAMPLE1 SAMPLE2 SAMPLE_SIMPLESKY SAMPLE_COMPLEXSKY \
                 MULTI/ASSAULTMAP/SAMPLE_AM MULTI/DEATHMATCH/SAMPLE_DM; do
            name=$(basename "$w")
            for side in ship ours; do
                mkdir -p "$dir/$side/$case/$name"
                cp "$dir/worlds/WORLDS/$w.ed" "$dir/$side/$case/$name/$name.ed"
                printf -- '%s -logfile -skipdialog \n\n' "$opts" > "$dir/$side/$case/$name/processor.opt"
                if ! run_tool_until 900 "$dir/$side/$case/$name" "${name}_0.log" '^Light table size' \
                        "$(winpath "$dir/$side/bin/Processor.exe")" "$(winpath "$dir/$side/$case/$name/$name")"; then
                    fail "$case/$name: the $side exe did not finish"; wine_tail "$dir/$side/$case/$name"
                    continue 2
                fi
                [ -f "$dir/$side/$case/$name/$name.dat" ] || { fail "$case/$name: the $side exe wrote no .dat"; wine_tail "$dir/$side/$case/$name"; continue 2; }
            done
            if cmp -s "$dir/ship/$case/$name/$name.dat" "$dir/ours/$case/$name/$name.dat"; then
                pass "$case/$name.dat ($(stat -c %s "$dir/ours/$case/$name/$name.dat") bytes): identical"
            else
                fail "$case/$name.dat differs: $(cmp "$dir/ship/$case/$name/$name.dat" "$dir/ours/$case/$name/$name.dat" 2>&1 | head -1)"
            fi
        done
    done
}

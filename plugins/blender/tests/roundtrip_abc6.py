#!/usr/bin/env python3
"""Round-trip every LithTech 1 .abc (v6) under some folders: read it,
convert it to the shared model, write it back, compare bytes. No Blender.

    roundtrip_abc6.py <dir> ...   (folders of .abc files extracted from
                                   Shogo's, Blood 2's or KISS's .rez)

Reports how many came back byte-identical, how many carry chunks v6 doesn't
define (KISS's TransformInfo, kept as stored), and lists every failure;
.abc files of other versions are counted and skipped.
"""
import sys
import traceback
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from lithtech import abc6  # noqa: E402


def main():
    dirs = [Path(a) for a in sys.argv[1:]]
    if not dirs:
        sys.exit(__doc__)
    files = sorted(p for d in dirs for p in d.rglob("*") if p.suffix.lower() == ".abc")
    same = extra = other = 0
    failures = []
    for p in files:
        data = p.read_bytes()
        if not abc6.is_model(data):
            other += 1
            continue
        try:
            m = abc6.read(data)
            abc6.to_abc(m)
            out = abc6.write(m)
        except Exception as e:
            failures.append((p, "%s: %s" % (type(e).__name__, e)))
            if len(failures) <= 3:
                traceback.print_exc()
            continue
        extra += bool(m.extra_chunks)
        if out == data:
            same += 1
        else:
            failures.append((p, "differs (%d vs %d bytes)" % (len(out), len(data))))
    total = len(files) - other
    print("%d of %d v6 models byte-identical (%d with chunks v6 doesn't define); %d other .abc skipped"
          % (same, total, extra, other))
    for p, why in failures:
        print("FAIL %s: %s" % (p, why))
    sys.exit(1 if failures or not total else 0)


if __name__ == "__main__":
    main()

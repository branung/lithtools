#!/usr/bin/env python3
"""Compare two world .dat files (AvP2's version 70) as the processor tests
need: byte for byte, except the vertex colours a retail bug leaves
uninitialised.

    datcmp.py <shipped.dat> <ours.dat>

The physics BSP's T-junction splits keep uninitialised vertex colours
(RETAIL_BUGS.md, "The physics BSP's T-junction splits keep uninitialised
vertex colours"): MakeBsp sets every physics BSP vertex colour to 255, and
the polies the T-junction fix creates after it get whatever the heap held.
That garbage follows the heap, and the heap follows the processor's
lighting threads' timing, so two runs of the same exe can differ there.

So the exception is exactly that: a byte of a physics BSP vertex colour may
differ, whatever either file holds there; every other byte must be equal.
It cannot be narrower. Heap garbage can be 255 as well as anything else,
and a control run of the shipped exe from another path (tests/processor-
avp2.sh) gives polies that are all 255 in one run and garbage in the
other; nothing in the file says which polies the T-junction fix made. Each
file's world models and colour layout are read on their own, and must
agree.

Prints "identical" (raw, byte for byte) or "equal but for M physics BSP
vertex colour bytes in N polies (RETAIL_BUGS.md)" -- a normalised
comparison, not a raw one -- or where the first real difference is
(offset, and the world model it is in). Exits 0 if the files agree as
above, 1 if not, 2 on a file it can't read.
"""
import struct
import sys

WIF_PHYSICSBSP = 0x10   # de_world.h: (1<<4)


class R:
    def __init__(self, d, o):
        self.d, self.o = d, o

    def u(self, f):
        v = struct.unpack_from('<' + f, self.d, self.o)
        self.o += struct.calcsize('<' + f)
        return v if len(v) > 1 else v[0]

    def skip(self, n):
        self.o += n

    def string(self):
        n = self.u('H')
        v = self.d[self.o:self.o + n]
        self.o += n
        return v


def world_models(d):
    """(start, end) of each world model: the marker chain after the world
    tree layout, ending at the object list."""
    objp = struct.unpack_from('<I', d, 4)[0]
    o = 12 + 32
    o += 4 + struct.unpack_from('<I', d, o)[0] + 4 + 24
    for s in range(o, min(o + 200000, objp)):
        n = struct.unpack_from('<I', d, s)[0]
        if not 0 < n < 5000:
            continue
        p, out = s + 4, []
        for _ in range(n):
            nxt = struct.unpack_from('<I', d, p)[0]
            if not p < nxt <= objp:
                break
            out.append((p + 4, nxt))
            p = nxt
        else:
            if p == objp:
                return out
    raise ValueError('no world model chain')


def colour_offsets(d, start):
    """Per poly, the file offsets of a physics BSP world model's vertex
    colour bytes (empty for any other world model)."""
    r = R(d, start)
    r.skip(32)
    flags, _depth = r.u('II')
    r.string()
    c = dict(zip(['points', 'planes', 'surfaces', 'u04', 'polies', 'leaves', 'polyverts',
                  'vislistmem', 'vislistcount', 'nodes', 'leafpolies', 'animrefs'], r.u('12I')))
    if not flags & WIF_PHYSICSBSP:
        return []
    r.skip(9 * 4)
    nl, _nt = r.u('II')
    r.skip(nl)
    hdr = [r.u('BB') for _ in range(c['polies'])]
    for _ in range(c['leaves']):
        n = r.u('H')
        if n != 0xFFFF:
            for _ in range(n):
                r.u('H')
                r.skip(r.u('H'))
        else:
            r.u('H')
        r.skip(4 * r.u('I'))
        r.u('I')
    r.skip(16 * c['planes'])
    for _ in range(c['surfaces']):
        r.u('9fHIB3B')
        if r.u('B'):
            r.string()
            r.string()
        r.u('H')
    r.skip(12 * c['points'])
    out = []
    for i in range(c['polies']):
        r.skip(12 + 4)
        r.skip(4 * r.u('H'))
        r.skip(8 + 36)
        poly = []
        for _ in range(hdr[i][0] + hdr[i][1]):
            r.skip(2)
            poly.extend((r.o, r.o + 1, r.o + 2))
            r.skip(3)
        out.append(poly)
    return out


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    try:
        a, b = (open(p, 'rb').read() for p in sys.argv[1:])
    except OSError as e:
        print(e)
        return 2
    if a == b:
        print('identical')
        return 0
    if len(a) != len(b):
        print('differs: %d bytes, %d bytes' % (len(a), len(b)))
        return 1
    try:
        models_a, models_b = world_models(a), world_models(b)
        if models_a != models_b:
            print('differs: the world models are laid out differently')
            return 1
        polies_a = [p for s, _e in models_a for p in colour_offsets(a, s)]
        polies_b = [p for s, _e in models_b for p in colour_offsets(b, s)]
        if polies_a != polies_b:
            print('differs: the physics BSP colours are laid out differently')
            return 1
    except (ValueError, struct.error) as e:
        polies_a, models_a = [], []
        print('(could not parse the world models: %s)' % e)
    loose = {}
    for k, poly in enumerate(polies_a):
        for o in poly:
            loose[o] = k
    polies, nbytes = set(), 0
    for i in range(len(a)):
        if a[i] == b[i]:
            continue
        k = loose.get(i)
        if k is not None:
            polies.add(k)
            nbytes += 1
            continue
        where = next(('world model %d' % j for j, (s, e) in enumerate(models_a) if s <= i < e), 'outside the world models')
        print('differs at byte %d (%s): %02x, %02x' % (i, where, a[i], b[i]))
        return 1
    print('equal but for %d physics BSP vertex colour bytes in %d polies '
          '(RETAIL_BUGS.md)' % (nbytes, len(polies)))
    return 0

if __name__ == '__main__':
    sys.exit(main())

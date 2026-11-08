"""LithTech 1's .abc models, version 6 (Shogo, Blood 2, KISS Psycho Circus),
read, written back byte for byte, and converted to the shared ABC model.

    from lithtech import abc6
    raw = abc6.read(data)            # the file as stored
    data = abc6.write(raw)           # identical to the input
    model = abc6.to_abc(raw)         # an abc.Model, as abc.read gives

Standard library only. The file is a chunk list like the later versions',
but its chunks are different: Header (the token "MonolithExport Model File
v6" and the command string), Geometry, Nodes, Animation and AnimDims. Shogo
and Blood 2 ship exactly those five, in that order.

What v6 stores, and how to_abc turns it into what abc.read gives:

  * One mesh for the whole model, each vertex on ONE node, its position in
    that node's space at the model's first pose: frame 0 of animation 0.
    So to_abc gives each vertex a single weight of bias 1 and binds each
    node to its frame-0 transform.
  * The mesh's vertex table has the reduced-detail vertices after the full
    ones: Geometry's "NumLODs" is their count, NormalVerts the full count,
    and the triangles index only the full ones. to_abc keeps LOD 0.
  * No node count: nodes are depth first, each giving its number of
    children.
  * Keyframe rotations are stored conjugated, and LithTech 1 draws every
    model mirrored in z (its renderer negates the third column of the
    model-to-world matrix). The shared model is in the space the engine
    draws, as abc.read's models are, so to_abc negates z of every position
    and quaternion, which is the conjugate and the mirror together.
  * Quaternions are kept as stored, length and all: 211 keys in 23 of
    Shogo's demo models and 3,331 in 11 of Blood 2's are not unit (up to
    length 313). LithTech 1 normalizes only in quat_ConvertToMatrix (2/|q|^2,
    which abc.quat_to_matrix now does too) and interpolates the raw values,
    so a long key pulls a midpoint toward itself; normalizing here would
    lose that.
  * VERTEX ANIMATION. A "deformation" node lists vertices whose positions it
    keys per keyframe as three bytes each, decompressed as byte * Scale +
    Transform in the node's space (the track's two trailing vectors). Their
    Geometry floats are those bytes, not positions: the first pose is frame
    0 of animation 0's. to_abc keys them in Node.animated_vertices and
    Animation.vertex_frames, and abc.animated_vertices applies them. Where a
    node lists one vertex twice the later entry wins, as LithTech 1's own
    loader overwrites it.

The evidence for each of these is in the LithTech 1 port this was written
beside (github.com/branung/Lithtech_Disassembly): its loader
runtime/model/src/model_abc.cpp, its DECOMP_SHOGO.md F15 (the mirror) and
SHOGO_PORT.md 10.3 and 11.9 (node space, the conjugate), and the
pose check that holds its engine to the same rule.
"""

from dataclasses import dataclass, field
from typing import List, Optional, Tuple

from . import abc
from .binio import Reader, Writer

TOKEN = "MonolithExport Model File v6"
CHUNKS = ("Header", "Geometry", "Nodes", "Animation", "AnimDims")
END_OF_CHUNKS = 0xFFFFFFFF

NODE_NULL = 1
NODE_TRIS = 2
NODE_DEFORMATION = 4

Vec2 = Tuple[float, float]
Vec3 = Tuple[float, float, float]


class ABC6Error(abc.ABCError):
    pass


# --------------------------------------------------------------------- #
#  Data: the file as stored
# --------------------------------------------------------------------- #

@dataclass
class Triangle:
    uvs: Tuple[Vec2, Vec2, Vec2]          # per corner, v down as in DTX
    indices: Tuple[int, int, int]         # into Model.vertices
    normal: Tuple[int, int, int]          # face normal, signed bytes over 127


@dataclass
class Vertex:
    position: Vec3                        # in its node's space; see the module notes
    normal: Tuple[int, int, int]          # signed bytes over 127
    node: int                             # index into Model.nodes
    replacements: Tuple[int, int]         # LOD collapse targets


@dataclass
class Node:
    bounds_min: Vec3
    bounds_max: Vec3
    name: str
    index: int
    flags: int                            # NODE_NULL, NODE_TRIS, NODE_DEFORMATION
    animated_vertices: List[int]          # the "MD" list: vertex indices it keys
    child_count: int


@dataclass
class Keyframe:
    time: int                             # ms
    bounds_min: Vec3
    bounds_max: Vec3
    command: str


@dataclass
class Track:
    """One node's channel in one animation."""
    keys: List[Tuple[Vec3, Tuple[float, float, float, float]]]   # (position, quaternion) as stored
    vertex_bytes: bytes = b""             # 3 * len(animated_vertices) * keyframes
    scale: Vec3 = (1.0, 1.0, 1.0)         # vertex decompression
    translation: Vec3 = (0.0, 0.0, 0.0)


@dataclass
class Animation:
    name: str
    length: int
    bounds_min: Vec3
    bounds_max: Vec3
    keyframes: List[Keyframe]
    tracks: List[Track]                   # one per node, in node order


@dataclass
class Model:
    command_string: str = ""
    bounds_min: Vec3 = (0.0, 0.0, 0.0)
    bounds_max: Vec3 = (0.0, 0.0, 0.0)
    vertex_start: List[int] = field(default_factory=lambda: [0])   # VertexStartNum
    triangles: List[Triangle] = field(default_factory=list)
    normal_vertex_count: int = 0          # the full-detail vertices, first in `vertices`
    vertices: List[Vertex] = field(default_factory=list)
    nodes: List[Node] = field(default_factory=list)                # depth first
    animations: List[Animation] = field(default_factory=list)
    anim_dims: Optional[List[Vec3]] = None                         # None: no AnimDims chunk
    chunks: List[str] = field(default_factory=lambda: list(CHUNKS))

    def node_parents(self) -> List[int]:
        parents = [-1] * len(self.nodes)
        stack = []
        for i, n in enumerate(self.nodes):
            parents[i] = stack[-1][0] if stack else -1
            if stack:
                stack[-1][1] -= 1
            while stack and stack[-1][1] == 0:
                stack.pop()
            if n.child_count:
                stack.append([i, n.child_count])
        return parents


# --------------------------------------------------------------------- #
#  Reading and writing
# --------------------------------------------------------------------- #

def _chunks(data) -> List[Tuple[str, int, int]]:
    out = []
    pos = 0
    while True:
        r = Reader(data, pos)
        name = r.string()
        nxt = r.u32()
        end = len(data) if nxt == END_OF_CHUNKS else nxt
        if end > len(data) or end < r.pos:
            raise ABC6Error("bad .abc chunk offset")
        out.append((name, r.pos, end))
        if nxt == END_OF_CHUNKS:
            return out
        if nxt <= pos:
            raise ABC6Error("bad .abc chunk list")
        pos = nxt


def is_model(data) -> bool:
    """A v6 .abc: its first chunk is Header and starts with the v6 token."""
    try:
        r = Reader(data, 0)
        if r.string() != "Header":
            return False
        r.u32()
        return r.string() == TOKEN
    except (EOFError, UnicodeDecodeError, Exception):
        return False


def read(data) -> Model:
    try:
        chunks = _chunks(data)
    except EOFError:
        raise ABC6Error("truncated .abc chunk list")
    where = {c[0]: c for c in chunks}
    m = Model(chunks=[c[0] for c in chunks])
    for name in m.chunks:
        if name not in CHUNKS:
            raise ABC6Error("chunk %r is not one of v6's" % name)
    for need in ("Header", "Geometry", "Nodes", "Animation"):
        if need not in where:
            raise ABC6Error("no %s chunk; not a v6 .abc?" % need)
    try:
        r = Reader(data, where["Header"][1])
        if r.string() != TOKEN:
            raise ABC6Error("not a v6 .abc (no %r)" % TOKEN)
        m.command_string = r.string()

        r = Reader(data, where["Geometry"][1])
        m.bounds_min, m.bounds_max = r.vec3(), r.vec3()
        nlod = r.u32()
        m.vertex_start = list(r.unpack("%dH" % (nlod + 1)))
        for _ in range(r.u32()):
            f = r.unpack("6f3H3b")
            m.triangles.append(Triangle(((f[0], f[1]), (f[2], f[3]), (f[4], f[5])), f[6:9], f[9:12]))
        nverts = r.u32()
        m.normal_vertex_count = r.u32()
        for _ in range(nverts):
            f = r.unpack("3f3bB2H")
            m.vertices.append(Vertex(f[0:3], f[3:6], f[6], f[7:9]))

        r = Reader(data, where["Nodes"][1])
        remaining = 1
        while remaining:
            if len(m.nodes) > 4096:
                raise ABC6Error(".abc node tree too large")
            bmin, bmax = r.vec3(), r.vec3()
            name = r.string()
            index = r.u16()
            flags = r.u8()
            nmd = r.u32()
            md = list(r.unpack("%dH" % nmd)) if nmd else []
            nkids = r.u32()
            m.nodes.append(Node(bmin, bmax, name, index, flags, md, nkids))
            remaining += nkids - 1

        r = Reader(data, where["Animation"][1])
        for _ in range(r.u32()):
            name = r.string()
            length = r.u32()
            bmin, bmax = r.vec3(), r.vec3()
            keyframes = []
            for _k in range(r.u32()):
                t = r.u32()
                k0, k1 = r.vec3(), r.vec3()
                keyframes.append(Keyframe(t, k0, k1, r.string()))
            tracks = []
            for n in m.nodes:
                keys = []
                for _k in keyframes:
                    f = r.unpack("7f")
                    keys.append((f[0:3], f[3:7]))
                vb = r.raw(3 * len(n.animated_vertices) * len(keyframes)) if n.animated_vertices else b""
                tracks.append(Track(keys, vb, r.vec3(), r.vec3()))
            m.animations.append(Animation(name, length, bmin, bmax, keyframes, tracks))

        if "AnimDims" in where:
            r = Reader(data, where["AnimDims"][1])
            m.anim_dims = [r.vec3() for _ in m.animations]
    except EOFError:
        raise ABC6Error("truncated v6 .abc")
    for t in m.triangles:
        if max(t.indices) >= len(m.vertices):
            raise ABC6Error("a triangle indexes past the vertices")
    for n in m.nodes:
        if any(v >= len(m.vertices) for v in n.animated_vertices):
            raise ABC6Error("node %r animates a vertex past the vertices" % n.name)
    return m


def read_file(path) -> Model:
    with open(path, "rb") as f:
        return read(f.read())


def _payload(m: Model, name: str) -> bytes:
    w = Writer()
    if name == "Header":
        w.string(TOKEN)
        w.string(m.command_string)
    elif name == "Geometry":
        w.vec3(m.bounds_min)
        w.vec3(m.bounds_max)
        w.u32(len(m.vertex_start) - 1)
        w.pack("%dH" % len(m.vertex_start), *m.vertex_start)
        w.u32(len(m.triangles))
        for t in m.triangles:
            w.pack("6f3H3b", t.uvs[0][0], t.uvs[0][1], t.uvs[1][0], t.uvs[1][1], t.uvs[2][0], t.uvs[2][1],
                   *t.indices, *t.normal)
        w.u32(len(m.vertices))
        w.u32(m.normal_vertex_count)
        for v in m.vertices:
            w.pack("3f3bB2H", *v.position, *v.normal, v.node, *v.replacements)
    elif name == "Nodes":
        for n in m.nodes:
            w.vec3(n.bounds_min)
            w.vec3(n.bounds_max)
            w.string(n.name)
            w.pack("HBI", n.index, n.flags, len(n.animated_vertices))
            if n.animated_vertices:
                w.pack("%dH" % len(n.animated_vertices), *n.animated_vertices)
            w.u32(n.child_count)
    elif name == "Animation":
        w.u32(len(m.animations))
        for a in m.animations:
            w.string(a.name)
            w.u32(a.length)
            w.vec3(a.bounds_min)
            w.vec3(a.bounds_max)
            w.u32(len(a.keyframes))
            for k in a.keyframes:
                w.u32(k.time)
                w.vec3(k.bounds_min)
                w.vec3(k.bounds_max)
                w.string(k.command)
            if len(a.tracks) != len(m.nodes):
                raise ABC6Error("%s: %d tracks for %d nodes" % (a.name, len(a.tracks), len(m.nodes)))
            for n, tr in zip(m.nodes, a.tracks):
                if len(tr.keys) != len(a.keyframes):
                    raise ABC6Error("%s/%s: %d keys for %d keyframes" % (a.name, n.name, len(tr.keys), len(a.keyframes)))
                for p, q in tr.keys:
                    w.pack("7f", *p, *q)
                if n.animated_vertices:
                    if len(tr.vertex_bytes) != 3 * len(n.animated_vertices) * len(a.keyframes):
                        raise ABC6Error("%s/%s: vertex animation is the wrong size" % (a.name, n.name))
                    w.raw(tr.vertex_bytes)
                w.vec3(tr.scale)
                w.vec3(tr.translation)
    elif name == "AnimDims":
        dims = m.anim_dims or []
        if len(dims) != len(m.animations):
            raise ABC6Error("%d AnimDims for %d animations" % (len(dims), len(m.animations)))
        for d in dims:
            w.vec3(d)
    return w.getvalue()


def write(m: Model) -> bytes:
    w = Writer()
    for i, name in enumerate(m.chunks):
        body = _payload(m, name)
        w.string(name)
        last = i + 1 == len(m.chunks)
        w.u32(END_OF_CHUNKS if last else w.tell() + 4 + len(body))
        w.raw(body)
    return w.getvalue()


def write_file(m: Model, path):
    with open(path, "wb") as f:
        f.write(write(m))


# --------------------------------------------------------------------- #
#  The shared model
# --------------------------------------------------------------------- #

def _mirror(p) -> Vec3:
    return (p[0], p[1], -p[2])


def _quat(q):
    """A stored v6 quaternion in the engine's drawing space: z negated (the
    conjugate and the mirror together), its length kept. LithTech 1
    interpolates the raw values (quat_Slerp, which takes a plain lerp when
    the dot passes 1, as it does for a long one) and normalizes only when it
    builds the matrix, as abc.quat_to_matrix does."""
    return (q[0], q[1], -q[2], q[3])


def vertex_frame(track: Track, count: int, key: int) -> List[Vec3]:
    """A track's animated vertex positions at a keyframe, decompressed, in
    the node's space and the engine's drawing space."""
    sx, sy, sz = track.scale
    tx, ty, tz = track.translation
    b = track.vertex_bytes
    base = key * count * 3
    return [_mirror((b[base + 3 * j] * sx + tx, b[base + 3 * j + 1] * sy + ty, b[base + 3 * j + 2] * sz + tz))
            for j in range(count)]


def _apply(M, p) -> Vec3:
    return tuple(M[4 * i] * p[0] + M[4 * i + 1] * p[1] + M[4 * i + 2] * p[2] + M[4 * i + 3] for i in range(3))


def _rotate(M, n) -> Vec3:
    return tuple(M[4 * i] * n[0] + M[4 * i + 1] * n[1] + M[4 * i + 2] * n[2] for i in range(3))


def to_abc(m: Model) -> abc.Model:
    """The shared model (abc.Model), as abc.read gives a v9-12 file: one piece,
    one LOD, nodes bound at frame 0 of animation 0, in the engine's space."""
    parents = m.node_parents()
    nn = len(m.nodes)
    first = m.animations[0] if m.animations else None

    # Each node's frame-0 transform: the bind.
    identity = abc.quat_to_matrix((0.0, 0.0, 0.0), (0.0, 0.0, 0.0, 1.0))
    binds = []
    for i in range(nn):
        if first is None or not first.keyframes:
            local = identity
        else:
            p, q = first.tracks[i].keys[0]
            local = abc.quat_to_matrix(_mirror(p), _quat(q))
        binds.append(local if parents[i] < 0 else abc.matrix_multiply(binds[parents[i]], local))

    # Node-space positions: the stored ones, mirrored, and for an animated
    # vertex frame 0 of animation 0's (the later of two entries wins).
    local_pos = [_mirror(v.position) for v in m.vertices]
    if first is not None and first.keyframes:
        for i, n in enumerate(m.nodes):
            if n.animated_vertices:
                for j, p in enumerate(vertex_frame(first.tracks[i], len(n.animated_vertices), 0)):
                    local_pos[n.animated_vertices[j]] = p

    # LOD 0: the full-detail vertices, which are all the triangles use.
    count = m.normal_vertex_count
    if not 0 < count <= len(m.vertices) or any(max(t.indices) >= count for t in m.triangles):
        count = len(m.vertices)
    lod = abc.LOD()
    for vi in range(count):
        v = m.vertices[vi]
        node = v.node if v.node < nn else 0
        M = binds[node] if nn else identity
        n = _rotate(M, _mirror(tuple(c / 127.0 for c in v.normal)))
        lod.vertices.append(abc.Vertex([abc.Weight(node, local_pos[vi], 1.0)], 0, _apply(M, local_pos[vi]), n))
    for t in m.triangles:
        lod.triangles.append(abc.Triangle(tuple(t.indices), tuple(t.uvs)))

    out = abc.Model(version=12, command_string=m.command_string)
    out.source_format = "ABC v6"
    out.source_version = 6
    out.pieces.append(abc.Piece(name="Piece", lods=[lod]))

    nodes = [abc.Node(n.name, n.index, n.flags, binds[i],
                      animated_vertices=[v for v in n.animated_vertices if v < count])
             for i, n in enumerate(m.nodes)]
    for i, p in enumerate(parents):
        if p >= 0:
            nodes[p].children.append(nodes[i])
    out.root = nodes[0] if nodes else None
    # An animated vertex past LOD 0 (none is, in the files held) is dropped
    # from the node's list; keep the frames in step with what is left.
    keep = [[j for j, v in enumerate(n.animated_vertices) if v < count] for n in m.nodes]

    for ai, a in enumerate(m.animations):
        dims = m.anim_dims[ai] if m.anim_dims and ai < len(m.anim_dims) else \
            tuple((a.bounds_max[c] - a.bounds_min[c]) / 2.0 for c in range(3))
        oa = abc.Animation(a.name, dims, keyframes=[abc.Keyframe(k.time, k.command) for k in a.keyframes])
        oa.node_transforms = [[(_mirror(p), _quat(q)) for p, q in tr.keys] for tr in a.tracks]
        if any(n.animated_vertices for n in m.nodes):
            oa.vertex_frames = []
            for i, n in enumerate(m.nodes):
                if not n.animated_vertices:
                    oa.vertex_frames.append([])
                    continue
                frames = [vertex_frame(a.tracks[i], len(n.animated_vertices), k) for k in range(len(a.keyframes))]
                oa.vertex_frames.append([[f[j] for j in keep[i]] for f in frames])
        out.animations.append(oa)

    # What abc.read gives for a v9-12 file with no child models: the model
    # itself as ChildModels' first entry (save index 1, no relation) and its
    # animations' bindings, which v6 has no use for and export_abc writes.
    ident = ((0.0, 0.0, 0.0), (0.0, 0.0, 0.0, 1.0))
    out.child_models = [abc.ChildModel("", 1, [ident] * nn)]
    out.anim_bindings = [[abc.AnimBinding(a.name, a.dims, (0.0, 0.0, 0.0)) for a in out.animations]]
    return out

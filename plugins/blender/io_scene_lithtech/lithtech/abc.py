"""LithTech .abc models (versions 9 to 12: NOLF, AvP2, AvP2 Primal Hunt).

    from lithtech import abc
    model = abc.read_file("RAG.abc")     # or abc.read(data)
    data = abc.write(model)               # bytes, identical to the input

See plugins/blender/docs/ABC.md for the layout and the evidence for each
field. Standard library only, so it runs inside Blender's Python.

Conventions (the engine's, kept as stored):
  * Units are LithTech world units; the files name no unit (see ABC.md for
    typical sizes).
  * Left-handed, Y up (+X right, +Y up, +Z forward), as the engine renders.
  * Quaternions are (x, y, z, w), the order of LTRotation::m_Quat.
  * Matrices are 4x4, row-major as stored (m[row][col]); the translation is
    in the last column (m[0][3], m[1][3], m[2][3]).
  * Times are milliseconds.

Version handling is explicit: every field that depends on the version is
read and written under a `version >= n` test that names the version, so
later generations (.ltb, Shogo-era .abc) can sit beside it as their own
modules rather than more branches here.
"""

from dataclasses import dataclass, field
from typing import List, Optional, Tuple

from .binio import Reader, Writer

Vec2 = Tuple[float, float]
Vec3 = Tuple[float, float, float]
Quat = Tuple[float, float, float, float]   # x, y, z, w
Mat4 = Tuple[float, ...]                    # 16 floats, row-major

SUPPORTED_VERSIONS = (9, 10, 11, 12)

# The chunk order every shipped file has, and the order the engine finds
# them in (Model::LoadChildModels, commonmodel.cpp).
CHUNK_ORDER = ("Header", "Pieces", "Nodes", "ChildModels", "Animation",
               "Sockets", "AnimBindings")

END_OF_CHUNKS = 0xFFFFFFFF


class ABCError(Exception):
    pass


# --------------------------------------------------------------------- #
#  Data
# --------------------------------------------------------------------- #

@dataclass
class Weight:
    """One node's share of a vertex: Build 69's NewVertexWeight, a
    homogeneous point (x, y, z, bias) in the node's bind space. The vertex
    is the sum over its weights of node.bind_matrix @ (x, y, z, bias), so
    `location` is already scaled by `bias` (see skinned_position)."""
    node: int                 # index into Model.nodes (depth-first order)
    location: Vec3
    bias: float               # usually summing to 1 over a vertex; not always


@dataclass
class Vertex:
    weights: List[Weight]
    # m_iReplacement: the vertex this one collapses into in the next LOD.
    # Build 69 calls it dead outside LOD generation; kept as stored.
    replacement: int
    location: Vec3            # model space, at the skeleton's bind pose
    normal: Vec3


@dataclass
class Triangle:
    """Three corners; each has its own UV, so seams need no split verts."""
    indices: Tuple[int, int, int]      # into LOD.vertices
    uvs: Tuple[Vec2, Vec2, Vec2]       # (u, v), v down as in DTX


@dataclass
class LOD:
    triangles: List[Triangle] = field(default_factory=list)
    vertices: List[Vertex] = field(default_factory=list)
    texture_index: Optional[int] = None  # per-LOD material in imported DHNP models


@dataclass
class Piece:
    name: str
    texture_index: int = 0             # the skin slot (ModelEdit's "Texture")
    specular_power: float = 5.0
    specular_scale: float = 1.0
    lod_weight: float = 1.0            # version >= 10
    # A uint16 count and that many dwords the engine skips (always 0 in
    # shipped files); kept so unknown data round-trips.
    reserved: List[int] = field(default_factory=list)
    # lods[0] is the full mesh; lods[1:] follow Model.lod_distances.
    lods: List[LOD] = field(default_factory=list)


@dataclass
class Node:
    name: str
    index: int                         # the node's own index (uint16)
    flags: int                         # uint8; 1 = null/removable, 2 = tris
    bind_matrix: Mat4                  # model space -> the node, at bind pose
    children: List["Node"] = field(default_factory=list)
    # Vertex animation, which only LithTech 1's .abc v6 has (lithtech.abc6):
    # the vertices of pieces[0].lods[0] whose positions this node keys per
    # keyframe, in Animation.vertex_frames order. Empty in versions 9-12,
    # and write() ignores it.
    animated_vertices: List[int] = field(default_factory=list)


@dataclass
class WeightSet:
    """An animation blend mask: one weight per node."""
    name: str
    weights: List[float]


@dataclass
class ChildModel:
    """An entry of the ChildModels chunk. The first one is the model itself
    ("SELF" in the engine) and has no name in the file."""
    name: str                          # "" for the first (self) entry
    # m_SaveIndex: the child file's save count when it was added. The
    # engine compares it with the child's own; a mismatch means its
    # animation dims are stale.
    save_index: int
    # m_Relation: one (position, rotation) per node of THIS model, moving
    # the parent's skeleton points for this child's animations.
    relation: List[Tuple[Vec3, Quat]] = field(default_factory=list)


@dataclass
class Keyframe:
    time: int                          # ms
    command: str                       # frame string ("" if none)


@dataclass
class Animation:
    name: str
    dims: Vec3                         # the object's box half-sizes
    # m_ModelEditWeightSet (version >= 11): the weight set ModelEdit
    # stacks the animation with, -1 for none.
    modeledit_weight_set: int = -1
    interpolation_time: int = 200      # m_InterpolationMS (version >= 12)
    keyframes: List[Keyframe] = field(default_factory=list)
    # node_transforms[i][k] is node i's (position, rotation) at keyframe k,
    # relative to its parent; nodes in depth-first order as Model.nodes.
    node_transforms: List[List[Tuple[Vec3, Quat]]] = field(default_factory=list)
    # vertex_frames[i][k][j] is the node-space position, at keyframe k, of
    # node i's animated_vertices[j] (.abc v6 only; empty otherwise).
    vertex_frames: List[List[List[Vec3]]] = field(default_factory=list)


@dataclass
class Socket:
    node: int
    name: str
    rotation: Quat
    position: Vec3


@dataclass
class AnimBinding:
    """Per animation of a child model: the dims and the translation."""
    name: str
    dims: Vec3
    translation: Vec3


@dataclass
class Model:
    version: int = 12
    # The thirteen header counts, as stored. None makes write() compute them
    # (compute_header_counts); a read model keeps the file's.
    header_counts: Optional[List[int]] = None
    command_string: str = ""
    internal_radius: float = 0.0
    lod_distances: List[float] = field(default_factory=list)
    header_reserved: bytes = bytes(62)  # 62 bytes the engine skips
    pieces: List[Piece] = field(default_factory=list)
    root: Optional[Node] = None
    weight_sets: List[WeightSet] = field(default_factory=list)
    child_models: List[ChildModel] = field(default_factory=list)
    animations: List[Animation] = field(default_factory=list)
    sockets: List[Socket] = field(default_factory=list)
    # One list per child model, in ChildModels order.
    anim_bindings: List[List[AnimBinding]] = field(default_factory=list)
    source_format: str = ""
    source_version: Optional[int] = None

    @property
    def nodes(self) -> List[Node]:
        """Every node, depth-first, parent before children: the order node
        indices, weights and animation transforms use."""
        out = []
        if self.root is not None:
            stack = [self.root]
            while stack:
                n = stack.pop()
                out.append(n)
                stack.extend(reversed(n.children))
        return out

    def node_parents(self) -> List[int]:
        """Parent index of each node in `nodes` order (-1 for the root)."""
        nodes = self.nodes
        idx = {id(n): i for i, n in enumerate(nodes)}
        parents = [-1] * len(nodes)
        for n in nodes:
            for c in n.children:
                parents[idx[id(c)]] = idx[id(n)]
        return parents


# --------------------------------------------------------------------- #
#  Transforms
# --------------------------------------------------------------------- #

def quat_to_matrix(position: Vec3, rotation: Quat) -> Mat4:
    """A (position, (x, y, z, w)) pair as a 16-float row-major matrix.

    Scaled by 2/|q|^2, as every LithTech engine's quat_ConvertToMatrix is
    (LithTech 1, NOLF, Jupiter), so the quaternion needn't be unit: .abc v6
    stores thousands that aren't (abc6.py). For a unit one it is the plain
    formula."""
    x, y, z, w = rotation
    n = x * x + y * y + z * z + w * w
    s = 2.0 / n if n > 0 else 0.0
    return (1 - s * (y * y + z * z), s * (x * y - z * w), s * (x * z + y * w), position[0],
            s * (x * y + z * w), 1 - s * (x * x + z * z), s * (y * z - x * w), position[1],
            s * (x * z - y * w), s * (y * z + x * w), 1 - s * (x * x + y * y), position[2],
            0.0, 0.0, 0.0, 1.0)


def matrix_multiply(a: Mat4, b: Mat4) -> Mat4:
    return tuple(sum(a[4 * i + k] * b[4 * k + j] for k in range(4))
                 for i in range(4) for j in range(4))


def pose_matrices(m: Model, anim: Animation, keyframe: int) -> List[Mat4]:
    """Each node's model-space matrix at a keyframe, in `nodes` order: the
    parent-relative transforms composed down the tree. At the first key of
    a model's base animation this reproduces the bind matrices."""
    parents = m.node_parents()
    out: List[Mat4] = []
    for i, per_node in enumerate(anim.node_transforms):
        local = quat_to_matrix(*per_node[keyframe])
        out.append(local if parents[i] < 0 else matrix_multiply(out[parents[i]], local))
    return out


def skinned_position(m: Model, v: Vertex, matrices: Optional[List[Mat4]] = None) -> Vec3:
    """Sum of matrix @ (x, y, z, bias) over the vertex's weights. With the
    bind matrices (the default) this gives back v.location for the files
    whose biases sum to 1; pass pose_matrices(...) to pose the vertex."""
    if matrices is None:
        matrices = [n.bind_matrix for n in m.nodes]
    s = [0.0, 0.0, 0.0]
    for wt in v.weights:
        M = matrices[wt.node]
        x, y, z = wt.location
        for i in range(3):
            s[i] += M[4 * i] * x + M[4 * i + 1] * y + M[4 * i + 2] * z + M[4 * i + 3] * wt.bias
    return (s[0], s[1], s[2])


def animated_vertices(m: Model, anim: Animation, keyframe: int, lod: LOD) -> List[Vertex]:
    """pieces[0].lods[0]'s vertices at a keyframe, for a model whose nodes
    animate vertices (.abc v6): each animated vertex's one weight moved to
    its keyed node-space position, the rest as stored. skinned_position
    then places them as usual. Without vertex animation, lod.vertices."""
    if not anim.vertex_frames:
        return lod.vertices
    out = list(lod.vertices)
    for i, node in enumerate(m.nodes):
        if not node.animated_vertices or i >= len(anim.vertex_frames) or not anim.vertex_frames[i]:
            continue
        frame = anim.vertex_frames[i][keyframe]
        for j, vi in enumerate(node.animated_vertices):
            v = out[vi]
            out[vi] = Vertex([Weight(w.node, frame[j], w.bias) for w in v.weights],
                             v.replacement, v.location, v.normal)
    return out


# --------------------------------------------------------------------- #
#  Header counts
# --------------------------------------------------------------------- #

HEADER_COUNT_NAMES = (
    "keyframes",          # sum of keyframes over animations
    "parent_anims",       # animations in this file
    "nodes",
    "pieces",
    "child_models",       # including self
    "triangles",          # over every piece and LOD
    "vertices",           # over every piece and LOD
    "vertex_weights",
    "lods",               # 1 + len(lod_distances)
    "sockets",
    "weight_sets",
    "strings",            # distinct non-empty strings the engine interns
    "string_bytes",       # their lengths summed
)


def compute_header_counts(m: Model) -> List[int]:
    tris = verts = weights = 0
    for p in m.pieces:
        for lod in p.lods:
            tris += len(lod.triangles)
            verts += len(lod.vertices)
            weights += sum(len(v.weights) for v in lod.vertices)
    strings = set()
    for s in [m.command_string] + [n.name for n in m.nodes] + \
             [c.name for c in m.child_models] + \
             [a.name for a in m.animations] + \
             [k.command for a in m.animations for k in a.keyframes]:
        if s:
            strings.add(s)
    return [
        sum(len(a.keyframes) for a in m.animations),
        len(m.animations),
        len(m.nodes),
        len(m.pieces),
        len(m.child_models),
        tris,
        verts,
        weights,
        1 + len(m.lod_distances),
        len(m.sockets),
        len(m.weight_sets),
        len(strings),
        sum(len(s) for s in strings),
    ]


# --------------------------------------------------------------------- #
#  Reading
# --------------------------------------------------------------------- #

def read_chunks(data, start=0) -> List[Tuple[str, int, int]]:
    """(name, start of body, end of body) for each chunk, in file order.

    A chunk is its name (uint16 + bytes), the offset of the next chunk
    (uint32, 0xFFFFFFFF for the last), then the body (mod_FindChunk)."""
    out = []
    pos = start
    for _ in range(0x200):            # the engine's cap on the walk
        r = Reader(data, pos)
        name = r.string()
        nxt = r.u32()
        end = len(data) if nxt == END_OF_CHUNKS else nxt
        if not r.pos <= end <= len(data):
            raise ABCError("chunk %r has an invalid next offset" % name)
        out.append((name, r.pos, end))
        if nxt == END_OF_CHUNKS:
            return out
        pos = nxt
    raise ABCError("chunk list does not end")


def _read_lod(r: Reader) -> LOD:
    lod = LOD()
    for _ in range(r.u32()):
        u0, v0, i0, u1, v1, i1, u2, v2, i2 = r.unpack("ffHffHffH")
        lod.triangles.append(Triangle((i0, i1, i2), ((u0, v0), (u1, v1), (u2, v2))))
    for _ in range(r.u32()):
        nweights, replacement = r.unpack("HH")
        weights = []
        for _ in range(nweights):
            node, x, y, z, bias = r.unpack("Iffff")
            weights.append(Weight(node, (x, y, z), bias))
        loc = r.vec3()
        nrm = r.vec3()
        lod.vertices.append(Vertex(weights, replacement, loc, nrm))
    return lod


def _read_node(r: Reader) -> Node:
    # Iterative would avoid Python's recursion limit, but skeletons are a
    # few dozen deep at most.
    name = r.string()
    index = r.u16()
    flags = r.u8()
    mat = r.unpack("16f")
    node = Node(name, index, flags, mat)
    for _ in range(r.u32()):
        node.children.append(_read_node(r))
    return node


def read(data) -> Model:
    data = memoryview(data) if not isinstance(data, (bytes, bytearray)) else data
    chunks = read_chunks(data)
    names = tuple(c[0] for c in chunks)
    if names != CHUNK_ORDER:
        raise ABCError("unexpected chunks %r" % (names,))
    body = {name: (start, end) for name, start, end in chunks}
    m = Model()

    def section(name):
        return Reader(data, body[name][0])

    def done(name, r):
        if r.pos != body[name][1]:
            raise ABCError("%s: read %d bytes, chunk has %d" % (
                name, r.pos - body[name][0], body[name][1] - body[name][0]))

    # Header (Model::LoadHeader, Model::LoadChildModels)
    r = section("Header")
    m.version = r.u32()
    if m.version not in SUPPORTED_VERSIONS:
        raise ABCError("unsupported .abc version %d" % m.version)
    m.header_counts = list(r.unpack("13I"))
    m.command_string = r.string()
    m.internal_radius = r.f32()
    nlods = r.u16()
    m.header_reserved = r.raw(62)
    m.lod_distances = list(r.unpack("%df" % nlods)) if nlods else []
    done("Header", r)

    # Pieces (ModelPiece::sub_1001c1a5 / sub_1001bf85)
    r = section("Pieces")
    r.u32()                            # total weights (the engine's pool); recomputed
    for _ in range(r.u32()):
        p = Piece(name="")
        p.texture_index = r.u16()
        p.specular_power = r.f32()
        p.specular_scale = r.f32()
        if m.version >= 10:
            p.lod_weight = r.f32()
        nres = r.u16()
        p.reserved = list(r.unpack("%dI" % nres)) if nres else []
        p.name = r.string()
        p.lods = [_read_lod(r) for _ in range(1 + nlods)]
        m.pieces.append(p)
    done("Pieces", r)

    # Nodes, then the weight sets (CModelSub20C::sub_1001bd54,
    # Model::sub_1001c560)
    r = section("Nodes")
    m.root = _read_node(r)
    for _ in range(r.u32()):
        name = r.string()
        n = r.u32()
        m.weight_sets.append(WeightSet(name, list(r.unpack("%df" % n)) if n else []))
    done("Nodes", r)
    nnodes = len(m.nodes)

    # ChildModels (LoadChildModels, CModelSub1D4::sub_1001bf0d)
    r = section("ChildModels")
    for i in range(r.u32()):
        name = r.string() if i > 0 else ""
        save_index = r.u32()
        relation = [(r.vec3(), r.quat()) for _ in range(nnodes)]
        m.child_models.append(ChildModel(name, save_index, relation))
    done("ChildModels", r)

    # Animation (AnimInfo::sub_1001bec9, CUnknownD9E4::sub_1001bc69,
    # CUnknownD974::sub_1001bb37)
    r = section("Animation")
    nodes = m.nodes
    for _ in range(r.u32()):
        a = Animation(name="", dims=r.vec3())
        a.name = r.string()
        if m.version >= 11:
            a.modeledit_weight_set = r.i32()
        if m.version >= 12:
            a.interpolation_time = r.u32()
        for _ in range(r.u32()):
            t = r.u32()
            a.keyframes.append(Keyframe(t, r.string()))
        nkeys = len(a.keyframes)
        # The file walks the node tree depth-first; that is nodes order.
        a.node_transforms = [[(r.vec3(), r.quat()) for _ in range(nkeys)]
                             for _ in nodes]
        m.animations.append(a)
    done("Animation", r)

    # Sockets (Model::LoadSockets)
    r = section("Sockets")
    for _ in range(r.u32()):
        node = r.u32()
        name = r.string()
        m.sockets.append(Socket(node, name, r.quat(), r.vec3()))
    done("Sockets", r)

    # AnimBindings (Model::sub_1001c36c): one list per child model.
    r = section("AnimBindings")
    for _ in m.child_models:
        binds = []
        for _ in range(r.u32()):
            name = r.string()
            binds.append(AnimBinding(name, r.vec3(), r.vec3()))
        m.anim_bindings.append(binds)
    done("AnimBindings", r)

    return m


def read_file(path) -> Model:
    with open(path, "rb") as f:
        return read(f.read())


# --------------------------------------------------------------------- #
#  Writing
# --------------------------------------------------------------------- #

def _write_lod(w: Writer, lod: LOD):
    w.u32(len(lod.triangles))
    for t in lod.triangles:
        (u0, v0), (u1, v1), (u2, v2) = t.uvs
        i0, i1, i2 = t.indices
        w.pack("ffHffHffH", u0, v0, i0, u1, v1, i1, u2, v2, i2)
    w.u32(len(lod.vertices))
    for v in lod.vertices:
        w.pack("HH", len(v.weights), v.replacement)
        for wt in v.weights:
            w.pack("Iffff", wt.node, wt.location[0], wt.location[1],
                   wt.location[2], wt.bias)
        w.vec3(v.location)
        w.vec3(v.normal)


def _write_node(w: Writer, n: Node):
    w.string(n.name)
    w.u16(n.index)
    w.u8(n.flags)
    w.pack("16f", *n.bind_matrix)
    w.u32(len(n.children))
    for c in n.children:
        _write_node(w, c)


def write(m: Model) -> bytes:
    if m.version not in SUPPORTED_VERSIONS:
        raise ABCError("cannot write .abc version %d" % m.version)
    nlods = len(m.lod_distances)
    for p in m.pieces:
        if len(p.lods) != 1 + nlods:
            raise ABCError("piece %r has %d LODs, the model %d" % (
                p.name, len(p.lods), 1 + nlods))
    if len(m.header_reserved) != 62:
        raise ABCError("header_reserved must be 62 bytes")
    if len(m.anim_bindings) != len(m.child_models):
        raise ABCError("anim_bindings needs one list per child model")
    nodes = m.nodes
    counts = m.header_counts if m.header_counts is not None else compute_header_counts(m)

    w = Writer()
    link = None

    def begin(name):
        nonlocal link
        if link is not None:
            w.patch_u32(link, w.tell())
        w.string(name)
        link = w.tell()
        w.u32(END_OF_CHUNKS)

    begin("Header")
    w.u32(m.version)
    w.pack("13I", *counts)
    w.string(m.command_string)
    w.f32(m.internal_radius)
    w.u16(nlods)
    w.raw(m.header_reserved)
    if nlods:
        w.pack("%df" % nlods, *m.lod_distances)

    begin("Pieces")
    w.u32(sum(len(v.weights) for p in m.pieces for l in p.lods for v in l.vertices))
    w.u32(len(m.pieces))
    for p in m.pieces:
        w.u16(p.texture_index)
        w.f32(p.specular_power)
        w.f32(p.specular_scale)
        if m.version >= 10:
            w.f32(p.lod_weight)
        w.u16(len(p.reserved))
        for x in p.reserved:
            w.u32(x)
        w.string(p.name)
        for lod in p.lods:
            _write_lod(w, lod)

    begin("Nodes")
    if m.root is None:
        raise ABCError("a model needs a root node")
    _write_node(w, m.root)
    w.u32(len(m.weight_sets))
    for ws in m.weight_sets:
        w.string(ws.name)
        w.u32(len(ws.weights))
        if ws.weights:
            w.pack("%df" % len(ws.weights), *ws.weights)

    begin("ChildModels")
    w.u32(len(m.child_models))
    for i, c in enumerate(m.child_models):
        if i > 0:
            w.string(c.name)
        w.u32(c.save_index)
        if len(c.relation) != len(nodes):
            raise ABCError("child model %d needs one relation per node" % i)
        for pos, rot in c.relation:
            w.vec3(pos)
            w.quat(rot)

    begin("Animation")
    w.u32(len(m.animations))
    for a in m.animations:
        w.vec3(a.dims)
        w.string(a.name)
        if m.version >= 11:
            w.i32(a.modeledit_weight_set)
        if m.version >= 12:
            w.u32(a.interpolation_time)
        w.u32(len(a.keyframes))
        for k in a.keyframes:
            w.u32(k.time)
            w.string(k.command)
        if len(a.node_transforms) != len(nodes):
            raise ABCError("animation %r needs transforms for every node" % a.name)
        for per_node in a.node_transforms:
            if len(per_node) != len(a.keyframes):
                raise ABCError("animation %r needs one transform per keyframe" % a.name)
            for pos, rot in per_node:
                w.vec3(pos)
                w.quat(rot)

    begin("Sockets")
    w.u32(len(m.sockets))
    for s in m.sockets:
        w.u32(s.node)
        w.string(s.name)
        w.quat(s.rotation)
        w.vec3(s.position)

    begin("AnimBindings")
    for binds in m.anim_bindings:
        w.u32(len(binds))
        for b in binds:
            w.string(b.name)
            w.vec3(b.dims)
            w.vec3(b.translation)

    return w.getvalue()


def write_file(m: Model, path):
    with open(path, "wb") as f:
        f.write(write(m))

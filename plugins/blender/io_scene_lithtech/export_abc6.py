"""Export an armature and its meshes as LithTech 1's .abc version 6 (Shogo,
Blood 2, KISS Psycho Circus), vertex animation included.

The inverse of import_abc for a v6 file. Every value is worked out from the
scene; where it still agrees with what the importer kept (the lt_v6_*
properties and attributes, see import_abc._keep_v6), the kept value is
written instead, exactly, as export_abc does for lt_location and the key
strings. So a v6 model imported and not edited is written back byte for
byte, and an edit changes only what it touches.

What v6 holds, and where it comes from:
  * ONE mesh, each vertex on ONE node, in that node's space at the first
    pose. The meshes the armature deforms (export_abc's rule) are written
    as one, in order. A vertex weighted to several bones goes to the
    heaviest; one with none to the bone its object is parented to, else
    the root. Triangles only.
  * the bones, numbered as export_abc numbers them.
  * the actions, found and keyed as export_abc finds and keys them. LithTech
    1 has no separate bind pose: it is the FIRST key of the FIRST
    animation, so that key should be the armature's rest pose (the
    importer makes it so) or the mesh comes out bent.
  * VERTEX ANIMATION: the mesh's shape keys "<action>|<key>"
    (import_abc.shape_key_name), at the bind pose, for each node's
    "animated vertices". A node keeps the list it was imported with while
    the mesh's faces are unchanged; a node with a vertex that moves in any
    frame lists every vertex on it, as every such node in the shipped files
    does. Each frame is stored as three bytes
    per vertex, byte * scale + translation in the node's space: the kept
    scale and translation while every byte still fits, else new ones that
    span the frames.
  * bounds (the model's, each node's, each animation's and keyframe's): the
    kept ones while what they bound is unchanged, else worked out by the
    rule most shipped files follow (measured on Shogo's and Blood 2's
    demos: the posed vertices' box for a keyframe, 637 of 749; the first
    pose's for the model, 185 of 200; the keyframes' union for an
    animation, 139 of 215; a node's own vertices for a node, 1,317 of
    1,320).
  * the reduced-detail vertices, which Blender doesn't show: kept while the
    mesh's faces and vertex count are unchanged (they refer to its vertices
    by number), else dropped (NumLODs 0, as 326 of the 414 v6 models held
    ship), and the vertices' LOD replacements with them.

Skins are not written: LithTech 1's textures are not the DTX version
textures.save_image writes.
"""

import json
import math

import bpy
from mathutils import Matrix, Vector

from .lithtech import abc, abc6
from . import convert
from . import export_abc
from . import import_abc

ExportError = export_abc.ExportError

POS_TOL = 1e-4          # LithTech units, plus POS_REL of the value
POS_REL = 1e-6
QUAT_TOL = 1e-4         # per component, between unit quaternions
CO_TOL = 1e-6           # Blender units: a vertex not moved since import
MOVE_TOL = 1e-5         # a shape key's vertex that differs from the Basis


def _unmirror(p):
    return (p[0], p[1], -p[2])


def _v6_quat(q):
    """From the shared model's drawing space back to the file's (abc6._quat
    is its own inverse)."""
    return (q[0], q[1], -q[2], q[3])


def _f32(x):
    import struct
    return struct.unpack("<f", struct.pack("<f", x))[0]


def _close_pos(a, b):
    return all(abs(x - y) <= POS_TOL + POS_REL * abs(y) for x, y in zip(a, b))


def _close_quat(stored, q):
    """A stored (perhaps not unit) quaternion and a unit one, as rotations."""
    n = math.sqrt(sum(c * c for c in stored))
    if n == 0.0:
        return False
    s = [c / n for c in stored]
    if sum(x * y for x, y in zip(s, q)) < 0.0:
        s = [-c for c in s]
    return all(abs(x - y) <= QUAT_TOL for x, y in zip(s, q))


def _point_attr(me, name, kind, width, domain='POINT'):
    a = me.attributes.get(name)
    n = {'POINT': len(me.vertices), 'CORNER': len(me.loops), 'FACE': len(me.polygons)}[domain]
    if a is None or a.domain != domain or a.data_type != kind or len(a.data) != n:
        return None
    out = [0] * (n * width) if kind == 'INT' else [0.0] * (n * width)
    a.data.foreach_get({'FLOAT_VECTOR': "vector", 'FLOAT2': "vector"}.get(kind, "value"), out)
    return out


def _basis_co(me):
    out = [0.0] * (3 * len(me.vertices))
    if me.shape_keys is not None and me.shape_keys.reference_key is not None:
        me.shape_keys.reference_key.data.foreach_get("co", out)
    else:
        me.vertices.foreach_get("co", out)
    return out


class _Mesh:
    """One mesh object's part of the v6 vertex table."""

    def __init__(self, obj, arm_obj, arm_inv, scale, node_index, node_binv, bind_same):
        me = obj.data
        self.obj, self.me = obj, me
        self.to_arm = export_abc._to_armature(obj, arm_obj, arm_inv)
        self.scale, self.node_binv = scale, node_binv
        self.identity = export_abc._is_identity(self.to_arm)
        nv = len(me.vertices)
        self.count = nv
        for p in me.polygons:
            if p.loop_total != 3:
                raise ExportError("%s: v6 holds triangles only; triangulate it (face %d has %d corners)"
                                  % (obj.name, p.index, p.loop_total))
        self.co = _basis_co(me)

        # Each vertex's node: its heaviest vertex group that is a bone.
        default_node = 0
        if obj.parent_type == 'BONE' and obj.parent_bone in node_index:
            default_node = node_index[obj.parent_bone]
        group_node = {g.index: node_index.get(g.name) for g in obj.vertex_groups}
        self.node = []
        self.shared = 0
        for v in me.vertices:
            best, weight, used = default_node, -1.0, 0
            for g in v.groups:
                ni = group_node.get(g.group)
                if ni is None or g.weight <= 0.0:
                    continue
                used += 1
                if g.weight > weight:
                    best, weight = ni, g.weight
            self.node.append(best)
            self.shared += used > 1

        # What the importer kept, valid while the object sits where it was.
        # Blender's attributes follow their vertices, corners and faces
        # through edits; one an edit made gets zeros, which match nothing.
        kept = self.identity
        self.kept_pos = _point_attr(me, "lt_v6_position", 'FLOAT_VECTOR', 3) if kept else None
        self.kept_normal = _point_attr(me, "lt_v6_normal", 'FLOAT_VECTOR', 3) if kept else None
        self.kept_node = _point_attr(me, "lt_v6_node", 'INT', 1) if kept else None
        self.kept_co = _point_attr(me, "lt_v6_import_co", 'FLOAT_VECTOR', 3) if kept else None
        reps = [_point_attr(me, "lt_v6_replacement%d" % k, 'INT', 1) for k in range(2)] if kept else [None, None]
        self.kept_uv = _point_attr(me, "lt_v6_uv", 'FLOAT2', 2, 'CORNER') if kept else None
        self.kept_face = _point_attr(me, "lt_v6_face_normal", 'FLOAT_VECTOR', 3, 'FACE') if kept else None
        self.face_imported = _point_attr(me, "lt_v6_face", 'INT', 1, 'FACE') if kept else None

        # Triangles, back in LithTech's winding (see import_abc).
        tris = []
        loops = [0] * len(me.loops)
        me.loops.foreach_get("vertex_index", loops)
        for p in me.polygons:
            ls = list(range(p.loop_start, p.loop_start + 3))
            tris.append((ls[0], ls[2], ls[1]))
        self.tri_loops = tris
        self.tri_indices = [tuple(loops[li] for li in t) for t in tris]
        # The faces and the vertex numbering as imported: what the
        # reduced-detail vertices and the replacements refer to.
        self.faces_same = kept and int(obj.get("lt_v6_vertex_count", -1)) == nv and \
            obj.get("lt_v6_triangle_crc") == import_abc.triangle_crc([i for t in self.tri_indices for i in t])

        # A vertex is unchanged while it is on the node it was imported on,
        # that node's bind is unchanged, and it sits where it was: for most,
        # where its stored floats put it; for an animated vertex, whose
        # floats are not a position, where the importer put it.
        self.pos_same = [False] * nv
        self.co_same = [False] * nv
        if self.kept_pos is not None and self.kept_node is not None and self.kept_co is not None:
            for vi in range(nv):
                if self.kept_node[vi] != self.node[vi] or not bind_same[self.node[vi]]:
                    continue
                kp = self.kept_pos[3 * vi:3 * vi + 3]
                self.pos_same[vi] = _close_pos(self.local(vi), kp)
                self.co_same[vi] = all(abs(self.co[3 * vi + c] - self.kept_co[3 * vi + c]) <=
                                       CO_TOL * max(1.0, abs(self.kept_co[3 * vi + c])) for c in range(3))
        self.unchanged = list(self.pos_same)
        self.reps = [(reps[0][vi], reps[1][vi]) if self.faces_same and reps[0] and reps[1] else (0, 0)
                     for vi in range(nv)]
        self.corner_same = None

        # Normals: a vertex all of whose corners keep the normal the importer
        # gave them keeps its stored bytes.
        me_n = [Vector((0.0, 0.0, 0.0)) for _ in range(nv)]
        cn = me.corner_normals
        corner_same = [True] * nv
        self.corner_same = corner_same
        imp = _point_attr(me, "lt_import_normal", 'FLOAT_VECTOR', 3, 'CORNER')
        for li, vi in enumerate(loops):
            n = cn[li].vector
            me_n[vi] += n
            if imp is None or max(abs(imp[3 * li + c] - n[c]) for c in range(3)) >= 1e-4:
                corner_same[vi] = False
        normal_mat = self.to_arm.to_3x3().inverted_safe().transposed()
        self.blender_normal = []
        for vi in range(nv):
            n = me_n[vi] if me_n[vi].length > 0 else me.vertices[vi].normal
            self.blender_normal.append(convert.normal_from_blender((normal_mat @ n).normalized()))

    def mark_animated(self, local_vertices):
        """The vertices a node animates are unchanged where they sit as
        imported (see __init__)."""
        for vi in local_vertices:
            self.unchanged[vi] = self.co_same[vi]

    def normal_kept(self, vi):
        return self.kept_normal is not None and self.corner_same[vi] and self.unchanged[vi]

    def lt_point(self, co3):
        """A Blender-space point of this mesh in the armature's LithTech space."""
        return convert.vec_from_blender(self.to_arm @ Vector(co3), self.scale)

    def local(self, vi, co3=None, node=None):
        """A vertex (or co3 for it) in its node's space as v6 stores it."""
        node = self.node[vi] if node is None else node
        p = self.lt_point(self.co[3 * vi:3 * vi + 3] if co3 is None else co3)
        return _unmirror(tuple(self.node_binv[node] @ Vector(p)))

    def shape_frames(self, action_name, nkeys):
        """{key: flat co} for this action's frames that exist."""
        keys = self.me.shape_keys
        out = {}
        if keys is None:
            return out
        for k in range(nkeys):
            kb = keys.key_blocks.get(import_abc.shape_key_name(action_name, k))
            if kb is None or len(kb.data) != self.count:
                continue
            flat = [0.0] * (3 * self.count)
            kb.data.foreach_get("co", flat)
            out[k] = flat
        return out


def _box(points):
    if not points:
        return (0.0, 0.0, 0.0), (0.0, 0.0, 0.0)
    return (tuple(_f32(min(p[c] for p in points)) for c in range(3)),
            tuple(_f32(max(p[c] for p in points)) for c in range(3)))


def _posed_box(sm, ai, k):
    a = sm.animations[ai]
    lod = sm.pieces[0].lods[0]
    M = abc.pose_matrices(sm, a, k)
    return _box([_unmirror(abc.skinned_position(sm, v, M)) for v in abc.animated_vertices(sm, a, k, lod)])


def _quantise(points, scale, translation):
    """Bytes for points with this scale and translation, or None if one
    doesn't fit."""
    out = bytearray()
    for p in points:
        for c in range(3):
            q = round((p[c] - translation[c]) / scale[c]) if scale[c] else 0
            if not 0 <= q <= 255:
                return None
            out.append(q)
    return bytes(out)


def _span(points):
    lo = [min(p[c] for p in points) for c in range(3)]
    hi = [max(p[c] for p in points) for c in range(3)]
    scale = tuple(_f32((hi[c] - lo[c]) / 255.0) or 1.0 for c in range(3))
    return scale, tuple(_f32(x) for x in lo)


def _vertex_bytes(lst, node, nkeys, kept, bone, frames, where):
    """A node's frames as v6 stores them: three bytes per entry of its list
    per keyframe, and the scale and translation that decode them: (bytes,
    scale, translation), the kept scale and translation while every byte
    fits them.

    An entry stays as kept where Blender can't show it (one shadowed by a
    later entry for the same vertex, or a frame with no shape key) while
    the list is the one imported."""
    kept_bytes = bytes.fromhex(kept[2]) if kept is not None else b""
    kept_list = list(bone.get("lt_v6_animated_vertices", [])) if bone is not None else []
    same_list = lst == kept_list and len(kept_bytes) == 3 * len(lst) * nkeys
    last = {v: j for j, v in enumerate(lst)}
    pts = []
    for k in range(nkeys):
        for j, v in enumerate(lst):
            m, mi, vi = where(v)
            flat = frames[mi].get(k)
            if same_list and (last[v] != j or flat is None):
                pts.append(None)
            elif flat is not None:
                pts.append(m.local(vi, flat[3 * vi:3 * vi + 3], node))
            else:
                pts.append(m.local(vi, None, node))

    def fill(points, scale, translation):
        out = bytearray()
        for e, p in enumerate(points):
            if p is None:
                out += kept_bytes[3 * e:3 * e + 3]
            else:
                b = _quantise([p], scale, translation)
                if b is None:
                    return None
                out += b
        return bytes(out)

    if kept is not None:
        data = fill(pts, tuple(kept[0]), tuple(kept[1]))
        if data is not None:
            return data, tuple(kept[0]), tuple(kept[1])
    # New ones, spanning every frame (a kept byte decoded with the kept ones).
    if kept is not None and same_list:
        s0, t0 = kept[0], kept[1]
        pts = [p if p is not None else
               tuple(kept_bytes[3 * e + c] * s0[c] + t0[c] for c in range(3)) for e, p in enumerate(pts)]
    scale, translation = _span(pts) if pts else ((1.0, 1.0, 1.0), (0.0, 0.0, 0.0))
    return fill(pts, scale, translation), scale, translation


def save(context, filepath, *, armature=None, scale=1.0, use_selection=False,
         export_animations=True, report=None):
    arm_obj = armature or export_abc.find_armature(context)
    if arm_obj is None:
        raise ExportError(export_abc.nothing_to_export(context))
    if context.object and context.object.mode != 'OBJECT':
        bpy.ops.object.mode_set(mode='OBJECT')
    notes = []
    fps = context.scene.render.fps / context.scene.render.fps_base

    nodes, node_bones, root = export_abc._build_skeleton(arm_obj, scale)
    nn = len(nodes)
    node_index = {b.name: i for i, b in enumerate(node_bones) if b is not None}
    node_bind = [Matrix((n.bind_matrix[0:4], n.bind_matrix[4:8], n.bind_matrix[8:12], n.bind_matrix[12:16]))
                 for n in nodes]
    node_binv = [m.inverted_safe() for m in node_bind]
    bind_same = []
    for b in node_bones:
        base = b.get("lt_bind_baseline") if b is not None else None
        bind_same.append(base is not None and all(abs(b.matrix_local[r][c] - base[r * 4 + c]) <= 1e-6
                                                  for r in range(4) for c in range(4)))
    parents = abc.Model(root=root).node_parents()

    # The mesh.
    candidates = context.selected_objects if use_selection else arm_obj.children_recursive
    if not use_selection:
        candidates = set(candidates) | {o for o in context.scene.objects
                                        if any(md.type == 'ARMATURE' and md.object == arm_obj
                                               for md in getattr(o, "modifiers", ()))}
    objs = sorted((o for o in export_abc._mesh_objects(arm_obj, candidates) if int(o.get("lt_lod", 0)) == 0),
                  key=lambda o: (o.get("lt_piece_index", 1 << 30), o.name))
    arm_inv = arm_obj.matrix_world.inverted()
    meshes = [_Mesh(o, arm_obj, arm_inv, scale, node_index, node_binv, bind_same) for o in objs]
    if not meshes:
        raise ExportError("no meshes found for armature %r" % arm_obj.name)
    if len(meshes) > 1:
        notes.append("%d meshes written as v6's one" % len(meshes))
    shared = sum(m.shared for m in meshes)
    if shared:
        notes.append("%d vertices weighted to several bones went to the heaviest (v6 holds one)" % shared)
    one = meshes[0] if len(meshes) == 1 else None

    raw = abc6.Model(command_string=arm_obj.get("lt_command_string", ""))
    offsets = []
    total = 0
    for m in meshes:
        offsets.append(total)
        total += m.count

    # Actions first: the animated vertices come from their frames.
    rest_locals = []
    for b in node_bones:
        if b is None:
            rest_locals.append(Matrix.Identity(4))
        elif b.parent is None:
            rest_locals.append(b.matrix_local.copy())
        else:
            rest_locals.append(b.parent.matrix_local.inverted() @ b.matrix_local)
    actions = export_abc._actions_for(arm_obj, node_bones) if export_animations else []
    anims = [export_abc._animation_from_action(a, arm_obj, nodes, node_bones, rest_locals, scale, fps)[0]
             for a in actions]
    if not anims:
        anims = [export_abc._bind_animation(nodes, "base", (0.0, 0.0, 0.0))]
        actions = [None]

    # Each action's frames, and the vertices that move in any of them.
    frames = []       # per action: per mesh: {key: flat co}
    moving = set()    # global vertex indices
    for act, a in zip(actions, anims):
        per = []
        for mi, m in enumerate(meshes):
            fr = m.shape_frames(act.name, len(a.keyframes)) if act is not None else {}
            per.append(fr)
            for flat in fr.values():
                for vi in range(m.count):
                    if any(abs(flat[3 * vi + c] - m.co[3 * vi + c]) > MOVE_TOL for c in range(3)):
                        moving.add(offsets[mi] + vi)
        frames.append(per)

    def where(g):
        mi = max(i for i, o in enumerate(offsets) if o <= g)
        return meshes[mi], mi, g - offsets[mi]

    node_of = [m.node[vi] for m in meshes for vi in range(m.count)]
    md = []
    for i, b in enumerate(node_bones):
        kept = list(b.get("lt_v6_animated_vertices", [])) if b is not None and one is not None and one.faces_same else []
        lst = [v for v in kept if v < total and node_of[v] == i]
        if lst or any(node_of[v] == i for v in moving):
            # A deformation node lists every vertex on it, as all 21 in the
            # shipped files held do (14 of them listing vertices that never
            # move), and as the Lithtech_Disassembly port's converter needs to build
            # its piece.
            have = set(lst)
            lst += [v for v in range(total) if node_of[v] == i and v not in have]
        md.append(lst)
    md_all = {v for lst in md for v in lst}
    for mi, m in enumerate(meshes):
        m.mark_animated([g - offsets[mi] for g in md_all if offsets[mi] <= g < offsets[mi] + m.count])
    whole_same = one is not None and one.faces_same and all(one.unchanged) and all(bind_same)

    # Geometry.
    for mi, m in enumerate(meshes):
        off = offsets[mi]
        for vi in range(m.count):
            g = off + vi
            if m.unchanged[vi]:
                # (An animated vertex's floats are not its position, and
                # LithTech 1 reads its frames instead, so they stay as kept
                # while it does.)
                pos = tuple(m.kept_pos[3 * vi:3 * vi + 3])
            else:
                pos = tuple(_f32(c) for c in m.local(vi))
            if m.normal_kept(vi):
                nb = tuple(int(c) for c in m.kept_normal[3 * vi:3 * vi + 3])
            else:
                R = node_binv[m.node[vi]].to_3x3()
                n = _unmirror(tuple(R @ Vector(m.blender_normal[vi])))
                nb = tuple(max(-127, min(127, int(round(c * 127.0)))) for c in n)
            raw.vertices.append(abc6.Vertex(pos, nb, m.node[vi], m.reps[vi]))
        uv_layer = m.me.uv_layers.active
        for fi, (tl, ti) in enumerate(zip(m.tri_loops, m.tri_indices)):
            uvs = []
            for li in tl:
                if uv_layer is not None:
                    u, v = uv_layer.uv[li].vector
                    uv = (u, 1.0 - v)
                else:
                    uv = (0.0, 0.0)
                if m.kept_uv is not None and all(abs(uv[c] - m.kept_uv[2 * li + c]) <= 1e-6 for c in range(2)):
                    uv = (m.kept_uv[2 * li], m.kept_uv[2 * li + 1])
                uvs.append(uv)
            if m.kept_face is not None and m.face_imported is not None and m.face_imported[fi] and \
                    all(m.unchanged[v] for v in ti):
                fn = tuple(int(c) for c in m.kept_face[3 * fi:3 * fi + 3])
            else:
                ps = [Vector(_unmirror(m.lt_point(m.co[3 * v:3 * v + 3]))) for v in ti]
                n = (ps[1] - ps[0]).cross(ps[2] - ps[0])
                # The sign most shipped faces have against their first pose
                # (7,124 of the 16,599 measured; 835 have the other, the
                # rest neither, and what LithTech 1 uses them for is not
                # known here).
                n = -n.normalized() if n.length > 0 else n
                fn = tuple(max(-127, min(127, int(round(c * 127.0)))) for c in n)
            raw.triangles.append(abc6.Triangle(tuple(uvs), tuple(off + v for v in ti), fn))
    raw.normal_vertex_count = total
    if one is not None and one.faces_same:
        # The reduced-detail vertices refer to the mesh's by number, so they
        # hold while its vertices and faces do.
        raw.vertex_start = list(one.obj.get("lt_v6_vertex_start", [0]))
        raw.normal_vertex_count = int(one.obj.get("lt_v6_normal_vertex_count", total))
        for r in json.loads(one.obj.get("lt_v6_reduced_json", "[]")):
            raw.vertices.append(abc6.Vertex(tuple(r[0:3]), tuple(int(c) for c in r[3:6]), int(r[6]),
                                            (int(r[7]), int(r[8]))))
    else:
        raw.vertex_start = [0]
        if one is not None and json.loads(one.obj.get("lt_v6_reduced_json", "[]")):
            notes.append("the reduced-detail vertices were dropped: the mesh's faces have changed")

    # Nodes.
    for i, (n, b) in enumerate(zip(nodes, node_bones)):
        flags = int(b.get("lt_flags", n.flags)) if b is not None else n.flags
        if md[i]:
            flags |= abc6.NODE_DEFORMATION
        bounds = list(b.get("lt_v6_bounds", [])) if b is not None and whole_same else []
        bmin, bmax = (tuple(bounds[0:3]), tuple(bounds[3:6])) if len(bounds) == 6 else (None, None)
        raw.nodes.append(abc6.Node(bmin, bmax, n.name, int(b.get("lt_v6_index", i)) if b is not None else i,
                                   flags, md[i], len(n.children)))

    # Animations.
    file_names = json.loads(arm_obj.get("lt_v6_node_names_json", "[]"))
    # Each node's place in the file it was imported from, by name; a name
    # some models use twice (Shogo's ORDOG has two "left_armu_spike_lower")
    # goes to its occurrences in order.
    by_name = {}
    for fi, nm in enumerate(file_names):
        by_name.setdefault(nm, []).append(fi)
    file_of = [by_name[n.name].pop(0) if by_name.get(n.name) else None for n in nodes]
    anim_bounds = []      # animations whose bounds are worked out below
    for ai, (act, a) in enumerate(zip(actions, anims)):
        nk = len(a.keyframes)
        times = [k.time for k in a.keyframes]
        get = act.get if act is not None else {}.get
        same_times = list(get("lt_v6_key_times", [])) == times
        stored = list(get("lt_v6_keys", [])) if same_times else []
        if len(stored) != 7 * len(file_names) * nk:
            stored = []
        kept_tracks = json.loads(get("lt_v6_tracks_json", "{}"))
        anim_same = whole_same and same_times
        tracks = []
        for i, n in enumerate(nodes):
            keys = []
            fi = file_of[i]
            for k, (p, q) in enumerate(a.node_transforms[i]):
                p6, q6 = _unmirror(p), _v6_quat(q)
                if stored and fi is not None:
                    base = 7 * (fi * nk + k)
                    sp, sq = tuple(stored[base:base + 3]), tuple(stored[base + 3:base + 7])
                    if _close_pos(p6, sp) and _close_quat(sq, q6):
                        keys.append((sp, sq))
                        continue
                anim_same = False
                keys.append((tuple(_f32(c) for c in p6), tuple(_f32(c) for c in q6)))
            tr = abc6.Track(keys)
            kt = kept_tracks.get(str(fi)) if fi is not None else None
            if kt is not None and not md[i]:
                tr.scale, tr.translation = tuple(kt[0]), tuple(kt[1])
            if md[i]:
                tr.vertex_bytes, tr.scale, tr.translation = _vertex_bytes(
                    md[i], i, nk, kt, node_bones[i], frames[ai], where)
                if kt is None or tr.vertex_bytes != bytes.fromhex(kt[2]) or \
                        (tr.scale, tr.translation) != (tuple(kt[0]), tuple(kt[1])):
                    anim_same = False
            tracks.append(tr)
        kb = list(get("lt_v6_key_bounds", []))
        b6 = list(get("lt_v6_bounds", []))
        kept_bounds = anim_same and len(kb) == 6 * nk and len(b6) == 6
        if kept_bounds:
            kfs = [abc6.Keyframe(k.time, tuple(kb[6 * j:6 * j + 3]), tuple(kb[6 * j + 3:6 * j + 6]), k.command)
                   for j, k in enumerate(a.keyframes)]
            bmin, bmax = tuple(b6[0:3]), tuple(b6[3:6])
        else:
            zero = (0.0, 0.0, 0.0)
            kfs = [abc6.Keyframe(k.time, zero, zero, k.command) for k in a.keyframes]
            bmin = bmax = zero
            anim_bounds.append(ai)
        length = int(get("lt_v6_length", times[-1] if times else 0)) if same_times else (times[-1] if times else 0)
        raw.animations.append(abc6.Animation(a.name, length, bmin, bmax, kfs, tracks))

    has_dims = all(int(act.get("lt_v6_has_dims", 1)) for act in actions if act is not None)
    raw.anim_dims = [tuple(_f32(c) for c in a.dims) for a in anims] if has_dims else None
    chunks = json.loads(arm_obj.get("lt_v6_chunks_json", "null")) or list(abc6.CHUNKS)
    if not has_dims:
        chunks = [c for c in chunks if c != "AnimDims"]
    elif "AnimDims" not in chunks:
        chunks.append("AnimDims")
    raw.chunks = chunks

    # Bounds that weren't kept, by the rule (see the module notes).
    gb = list(arm_obj.get("lt_v6_bounds", []))
    geo = not (whole_same and len(gb) == 6)
    if not geo:
        raw.bounds_min, raw.bounds_max = tuple(gb[0:3]), tuple(gb[3:6])
    holes = [i for i, n in enumerate(raw.nodes) if n.bounds_min is None]
    for i in holes:
        raw.nodes[i].bounds_min = raw.nodes[i].bounds_max = (0.0, 0.0, 0.0)
    if geo or holes or anim_bounds:
        sm = abc6.to_abc(raw)
        lod = sm.pieces[0].lods[0]
        if geo:
            raw.bounds_min, raw.bounds_max = _posed_box(sm, 0, 0) if raw.animations[0].keyframes else _box([])
        for i in holes:
            raw.nodes[i].bounds_min, raw.nodes[i].bounds_max = _box(
                [_unmirror(v.weights[0].location) for v in lod.vertices if v.weights[0].node == i])
        for ai in anim_bounds:
            a = raw.animations[ai]
            for k, kf in enumerate(a.keyframes):
                kf.bounds_min, kf.bounds_max = _posed_box(sm, ai, k)
            boxes = [(k.bounds_min, k.bounds_max) for k in a.keyframes] or [_box([])]
            a.bounds_min = tuple(min(b[0][c] for b in boxes) for c in range(3))
            a.bounds_max = tuple(max(b[1][c] for b in boxes) for c in range(3))

    # The first key of the first animation is v6's bind: say when it isn't
    # the rest pose the mesh was written against.
    if anims and anims[0].keyframes:
        a = anims[0]
        worst = 0.0
        for i in range(nn):
            p, q = a.node_transforms[i][0]
            local = abc.quat_to_matrix(p, q)
            M = Matrix((local[0:4], local[4:8], local[8:12], local[12:16]))
            rest = node_bind[i] if parents[i] < 0 else node_binv[parents[i]] @ node_bind[i]
            worst = max(worst, max(abs(M[r][c] - rest[r][c]) for r in range(3) for c in range(4)))
        if worst > 1e-3:
            notes.append("the first key of %r is not the rest pose (off by %.3g): LithTech 1 binds the mesh "
                         "to that key, so set it to the rest pose" % (a.name, worst))

    abc6.write_file(raw, filepath)
    if report is not None:
        for n in notes:
            report({'WARNING'}, n)
    return raw, [filepath], notes

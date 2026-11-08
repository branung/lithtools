"""Export an armature and its meshes as a LithTech .abc model.

The inverse of import_abc (see there for where each part of the model is
kept). Anything made in Blender works too:
  * every mesh parented to the armature, or deformed by it through an
    Armature modifier, is a piece (one per material if it has several);
    objects with lt_lod = n > 0 are LOD n of the piece named by lt_piece
  * vertex groups named after bones are the weights; a vertex with none
    goes wholly to the bone the object is parented to, else the root
  * empties parented to a bone are sockets
  * actions are the animations: those imported with this model (lt_model),
    else every action that animates this armature's bones; keys are taken
    where any of its F-curves has one, and pose markers become keyframe
    strings. Actions imported from child models (lt_child) stay in them.

The armature object's own transform is ignored: the model is written in
the armature's space.
"""

import json
import os

import bpy
from bpy_extras import anim_utils
from mathutils import Matrix, Vector

from .lithtech import abc
from . import convert
from . import import_abc
from . import textures


class ExportError(Exception):
    pass


def nothing_to_export(context):
    """Why find_armature found nothing, for the user."""
    arms = [o for o in context.scene.objects if o.type == 'ARMATURE']
    if len(arms) > 1:
        return ("There are %d armatures: select the one to export (or a mesh it deforms)" % len(arms))
    return ("Nothing to export: there's no model in the scene. Exporting writes the scene's "
            "model to a file; to open a model, use File > Import > LithTech Model")


def find_armature(context):
    obj = context.active_object
    if obj is not None:
        if obj.type == 'ARMATURE':
            return obj
        if obj.parent is not None and obj.parent.type == 'ARMATURE':
            return obj.parent
        for mod in getattr(obj, "modifiers", ()):
            if mod.type == 'ARMATURE' and mod.object is not None:
                return mod.object
    arms = [o for o in context.scene.objects if o.type == 'ARMATURE']
    return arms[0] if len(arms) == 1 else None


def _lt_name(id_, default):
    return id_.get("lt_name", default)


def _ordered_bones(arm):
    def key(b):
        return (b.get("lt_index", 1 << 30), b.name)
    out = []
    stack = sorted((b for b in arm.bones if b.parent is None), key=key, reverse=True)
    while stack:
        b = stack.pop()
        out.append(b)
        stack.extend(sorted(b.children, key=key, reverse=True))
    return out


def _build_skeleton(arm_obj, scale):
    """(Model nodes in depth-first order, the bone of each or None for a
    made-up root, the root Node)."""
    bones = _ordered_bones(arm_obj.data)
    if not bones:
        raise ExportError("the armature has no bones")
    roots = [b for b in bones if b.parent is None]

    nodes, node_bones = [], []
    by_bone = {}
    if len(roots) > 1:
        # LithTech has one root; give the bones one that doesn't move.
        top = abc.Node("root", 0, 1, tuple(Matrix.Identity(4)[i][j] for i in range(4) for j in range(4)))
        nodes.append(top)
        node_bones.append(None)
    for b in bones:
        n = abc.Node(_lt_name(b, b.name), len(nodes), int(b.get("lt_flags", 0)),
                     convert.matrix_from_blender(convert.bone_matrix(b), scale))
        if b.parent is not None:
            by_bone[b.parent.name].children.append(n)
        elif len(roots) > 1:
            nodes[0].children.append(n)
        by_bone[b.name] = n
        nodes.append(n)
        node_bones.append(b)
    # The depth-first order of the tree must be the order we numbered in.
    root = nodes[0]
    order = abc.Model(root=root).nodes
    assert [id(n) for n in order] == [id(n) for n in nodes]
    return nodes, node_bones, root


def _mesh_objects(arm_obj, objects):
    out = []
    for o in objects:
        if o.type != 'MESH':
            continue
        if o.parent == arm_obj or any(m.type == 'ARMATURE' and m.object == arm_obj
                                      for m in o.modifiers):
            out.append(o)
    return out


def _evaluated_mesh(obj, depsgraph):
    """The object's mesh with its modifiers except Armature ones, and
    without its vertex-animation frames (shape keys "<animation>|<key>"):
    playing an animation sets their values, and the mesh is written at
    rest."""
    toggled = []
    for m in obj.modifiers:
        if m.type == 'ARMATURE' and m.show_viewport:
            m.show_viewport = False
            toggled.append(m)
    keys = obj.data.shape_keys if obj.type == 'MESH' else None
    muted = [kb for kb in keys.key_blocks if "|" in kb.name and not kb.mute] if keys else []
    for kb in muted:
        kb.mute = True
    try:
        depsgraph.update()
        ev = obj.evaluated_get(depsgraph)
        me = ev.to_mesh(preserve_all_data_layers=True, depsgraph=depsgraph)
        me = me.copy()
        ev.to_mesh_clear()
    finally:
        for m in toggled:
            m.show_viewport = True
        for kb in muted:
            kb.mute = False
    return me


def _to_armature(obj, arm_obj, arm_inv):
    """The object's matrix in the armature's space. Taken from the parent
    link where there is one: objects in hidden collections (the LODs) don't
    get their world matrix updated."""
    if obj.parent == arm_obj and obj.parent_type == 'OBJECT':
        return obj.matrix_parent_inverse @ obj.matrix_basis
    return arm_inv @ obj.matrix_world


def _is_identity(m, tol=1e-6):
    return all(abs(m[i][j] - (1.0 if i == j else 0.0)) < tol for i in range(4) for j in range(4))


def _lods_from_object(obj, me, to_arm, scale, node_index, node_binv, default_node,
                      normalize):
    """One abc.LOD per material used by the mesh: {material index: LOD}.

    A LithTech vertex has one normal, so a Blender vertex whose corners have
    different normals becomes several. Vertices keep Blender's order (an
    imported mesh comes back numbered as it was), loose ones included.
    """
    me.calc_loop_triangles()
    uv_layer = me.uv_layers.active
    group_names = {g.index: g.name for g in obj.vertex_groups}
    rep_attr = me.attributes.get("lt_replacement")
    rep = None
    if rep_attr is not None and rep_attr.domain == 'POINT' and rep_attr.data_type == 'INT':
        rep = [0] * len(me.vertices)
        rep_attr.data.foreach_get("value", rep)

    normal_mat = to_arm.to_3x3().inverted_safe().transposed()
    corner_normals = me.corner_normals

    def point_attr(name, kind, width):
        a = me.attributes.get(name)
        if a is None or a.domain != 'POINT' or a.data_type != kind:
            return None
        out = [0] * (len(me.vertices) * width) if kind == 'INT' else \
            [0.0] * (len(me.vertices) * width)
        a.data.foreach_get("vector" if kind == 'FLOAT_VECTOR' else "value", out)
        return out

    # What the importer kept (see import_abc._create_mesh_object).
    stored_normal = point_attr("lt_normal", 'FLOAT_VECTOR', 3)
    stored_loc = point_attr("lt_location", 'FLOAT_VECTOR', 3)
    stored_weights = []
    k = 0
    while stored_loc is not None:
        nodes_k = point_attr("lt_weight%d_node" % k, 'INT', 1)
        if nodes_k is None:
            break
        stored_weights.append((nodes_k, point_attr("lt_weight%d_point" % k, 'FLOAT_VECTOR', 3),
                               point_attr("lt_weight%d_bias" % k, 'FLOAT', 1)))
        k += 1
    # The stored normals and points are in LithTech's space, so they only
    # apply while the object sits where it was imported.
    if not _is_identity(to_arm):
        stored_normal = stored_loc = None
        stored_weights = []

    def mesh_normal(vec):
        return convert.normal_from_blender((normal_mat @ vec).normalized())

    # A corner whose normal is still the one the importer saw takes the
    # vertex's stored normal exactly.
    unchanged = None
    imp = me.attributes.get("lt_import_normal")
    if stored_normal is not None and imp is not None and imp.domain == 'CORNER' and \
            imp.data_type == 'FLOAT_VECTOR' and len(imp.data) == len(me.loops):
        was = [0.0] * (3 * len(me.loops))
        imp.data.foreach_get("vector", was)
        now = [0.0] * (3 * len(me.loops))
        corner_normals.foreach_get("vector", now)
        unchanged = [max(abs(was[3 * i + c] - now[3 * i + c]) for c in range(3)) < 1e-4
                     for i in range(len(me.loops))]

    def lt_normal(vec, vi, li=None):
        if unchanged is not None and li is not None and unchanged[li]:
            return tuple(stored_normal[3 * vi:3 * vi + 3])
        return mesh_normal(vec)

    # Pass 1: the corners of each material's triangles, keyed by vertex
    # and normal.
    tris = {}          # material -> [(keys, uvs)]
    keys = {}          # material -> {key: (vertex index, normal, first seen)}
    used = set()
    for tri in me.loop_triangles:
        mi = tri.material_index
        kmap = keys.setdefault(mi, {})
        ks, uvs = [], []
        for li, vi in zip(tri.loops, tri.vertices):
            n_lt = lt_normal(corner_normals[li].vector, vi, li)
            key = (vi, round(n_lt[0], 5), round(n_lt[1], 5), round(n_lt[2], 5))
            if key not in kmap:
                kmap[key] = (vi, n_lt, len(kmap))
            ks.append(key)
            used.add(vi)
            if uv_layer is not None:
                u, v_ = uv_layer.uv[li].vector
                uvs.append((u, 1.0 - v_))
            else:
                uvs.append((0.0, 0.0))
        tris.setdefault(mi, []).append((ks, uvs))
    if not keys:
        keys[0] = {}
        tris[0] = []
    first = min(keys)
    for vi in range(len(me.vertices)):
        if vi not in used:
            # A loose vertex has no corners to edit: its stored normal if any.
            n_lt = tuple(stored_normal[3 * vi:3 * vi + 3]) if stored_normal is not None \
                else mesh_normal(me.vertices[vi].normal)
            keys[first][(vi, "loose")] = (vi, n_lt, len(keys[first]))

    # Pass 2: the vertices in Blender's order, then the triangles.
    lods = {}
    for mi, kmap in keys.items():
        lod = lods[mi] = abc.LOD()
        order = sorted(kmap, key=lambda k: (kmap[k][0], kmap[k][2]))
        index = {}
        for k in order:
            vi, n_lt, _ = kmap[k]
            index[k] = len(lod.vertices)
            v = me.vertices[vi]
            pos = convert.vec_from_blender(to_arm @ v.co, scale)
            weights = []
            for g in v.groups:
                ni = node_index.get(group_names.get(g.group))
                if ni is not None and g.weight > 1e-6:
                    weights.append([ni, g.weight])
            if not weights:
                weights = [[default_node, 1.0]]
            weights.sort()
            raw_weights = [tuple(w) for w in weights]
            if normalize:
                total = sum(w for _, w in weights)
                if total > 0.0 and abs(total - 1.0) > 1e-6:
                    for w in weights:
                        w[1] /= total
            pv = Vector(pos)
            lw = None
            if stored_weights and stored_weights[0][0][vi] >= 0 and \
                    max(abs(a - b) for a, b in zip(pos, stored_loc[3 * vi:3 * vi + 3])) < 1e-4:
                # An odd vertex that hasn't moved: its weights as they were,
                # while they're the same nodes with the same biases.
                old = [(nk[vi], tuple(pk[3 * vi:3 * vi + 3]), bk[vi])
                       for nk, pk, bk in stored_weights if nk[vi] >= 0]
                expect = {n_: b_ for n_, b_ in
                          import_abc.group_weights((n_, b_) for n_, _, b_ in old).items()
                          if b_ > 1e-6}
                have = dict(raw_weights)
                if set(expect) == set(have) and \
                        all(abs(expect[n_] - have[n_]) < 1e-6 for n_ in expect):
                    lw = [abc.Weight(n_, p_, b_) for n_, p_, b_ in old]
            if lw is None:
                lw = []
                for ni, bias in weights:
                    local = node_binv[ni] @ pv
                    lw.append(abc.Weight(ni, (local.x * bias, local.y * bias, local.z * bias),
                                         bias))
            lod.vertices.append(abc.Vertex(lw, rep[vi] if rep else 0, pos, n_lt))
        for ks, uvs in tris[mi]:
            idx = [index[k] for k in ks]
            # Back to LithTech's winding (see import_abc).
            lod.triangles.append(abc.Triangle((idx[0], idx[2], idx[1]),
                                              (uvs[0], uvs[2], uvs[1])))
    return lods


def _action_fcurves(action, arm_obj):
    slot = None
    ad = arm_obj.animation_data
    if ad is not None and ad.action == action and ad.action_slot is not None:
        slot = ad.action_slot
    if slot is None:
        slot = next((s for s in action.slots if s.target_id_type == 'OBJECT'
                     and s.name_display == arm_obj.name), None)
    if slot is None:
        slot = anim_utils.action_get_first_suitable_slot(action, 'OBJECT')
    if slot is None:
        return []
    bag = anim_utils.action_get_channelbag_for_slot(action, slot)
    return list(bag.fcurves) if bag is not None else []


def _actions_for(arm_obj, node_bones):
    name = arm_obj.name
    own = [a for a in bpy.data.actions if "lt_child" not in a and
           (a.get("lt_rig") == name or ("lt_rig" not in a and a.get("lt_model") == name))]
    if not own:
        paths = {'pose.bones["%s"]' % bpy.utils.escape_identifier(b.name)
                 for b in node_bones if b is not None}
        for a in bpy.data.actions:
            if "lt_child" in a:
                continue
            if "lt_rig" in a and a["lt_rig"] != name:
                continue
            if any(fc.data_path.split(".")[0] + "." + fc.data_path.split(".")[1]
                   in paths for fc in _action_fcurves(a, arm_obj)
                   if fc.data_path.startswith("pose.bones[")):
                own.append(a)
    return sorted(own, key=lambda a: (a.get("lt_index", 1 << 30), a.name))


def _bone_channels(fcurves):
    """{bone name: {prop: {index: fcurve}}}."""
    out = {}
    for fc in fcurves:
        p = fc.data_path
        if not p.startswith('pose.bones["'):
            continue
        end = p.find('"]', 12)
        bone = p[12:end].replace('\\"', '"').replace("\\\\", "\\")
        prop = p[end + 3:]
        out.setdefault(bone, {}).setdefault(prop, {})[fc.array_index] = fc
    return out


def _eval(chans, prop, frame, default):
    d = chans.get(prop)
    if not d:
        return default
    return [d[i].evaluate(frame) if i in d else default[i] for i in range(len(default))]


def _animation_from_action(action, arm_obj, nodes, node_bones, rest_locals, scale, fps):
    fcurves = _action_fcurves(action, arm_obj)
    frames = set()
    for fc in fcurves:
        for kp in fc.keyframe_points:
            frames.add(kp.co[0])
    markers = sorted(action.pose_markers, key=lambda m: m.frame)
    keyed = sorted(frames)
    for mk in markers:
        # Markers sit on whole frames; one within half a frame of a key
        # belongs to it.
        if not keyed or min(abs(f - mk.frame) for f in keyed) > 0.5 + 1e-6:
            frames.add(float(mk.frame))
    if not frames:
        frames = {0.0}
    frames = sorted(frames)
    # Imported actions keep their times; others start at 0.
    start = 0.0 if "lt_index" in action else frames[0]
    times = []
    for f in frames:
        t = int(round((f - start) * 1000.0 / fps))
        if not times or t != times[-1]:
            times.append(t)
    frames = [start + t * fps / 1000.0 for t in times]

    keyframes = [abc.Keyframe(t, "") for t in times]
    stored = json.loads(action.get("lt_key_strings_json", "[]"))
    sfps = action.get("lt_key_strings_fps", fps)
    expect = sorted((int(round(t * sfps / 1000.0)), c[:63]) for t, c in stored)
    if stored and abs(sfps - fps) < 1e-6 and \
            expect == sorted((mk.frame, mk.name) for mk in markers) and \
            all(t in times for t, _ in stored):
        for t, c in stored:
            k = keyframes[times.index(t)]
            k.command = c if not k.command else k.command + ";" + c
        markers = []
    for mk in markers:
        k = min(range(len(frames)), key=lambda i: abs(frames[i] - mk.frame))
        keyframes[k].command = mk.name if not keyframes[k].command \
            else keyframes[k].command + ";" + mk.name

    chans = _bone_channels(fcurves)
    transforms = []
    for ni, b in enumerate(node_bones):
        per = []
        if b is None:
            per = [((0.0, 0.0, 0.0), (0.0, 0.0, 0.0, 1.0))] * len(frames)
        else:
            c = chans.get(b.name, {})
            pb = arm_obj.pose.bones[b.name]
            parent_axes_inv = convert.bone_correction(b.parent).inverted() if b.parent else Matrix.Identity(4)
            axes = convert.bone_correction(b)
            for f in frames:
                loc = Vector(_eval(c, "location", f, [0.0, 0.0, 0.0]))
                if pb.rotation_mode == 'QUATERNION':
                    from mathutils import Quaternion
                    rot = Quaternion(_eval(c, "rotation_quaternion", f, [1.0, 0.0, 0.0, 0.0]))
                    rot.normalize()
                    rm = rot.to_matrix().to_4x4()
                elif pb.rotation_mode == 'AXIS_ANGLE':
                    from mathutils import Quaternion
                    aa = _eval(c, "rotation_axis_angle", f, [0.0, 0.0, 1.0, 0.0])
                    rm = Quaternion(aa[1:], aa[0]).to_matrix().to_4x4()
                else:
                    from mathutils import Euler
                    rm = Euler(_eval(c, "rotation_euler", f, [0.0, 0.0, 0.0]),
                               pb.rotation_mode).to_matrix().to_4x4()
                basis = Matrix.Translation(loc) @ rm
                local = parent_axes_inv @ rest_locals[ni] @ basis @ axes
                per.append(convert.transform_from_blender(local, scale))
        transforms.append(per)

    anim = abc.Animation(action.get("lt_name", action.name),
                         tuple(action.get("lt_dims", (24.0, 53.0, 24.0))),
                         int(action.get("lt_weight_set", -1)),
                         int(action.get("lt_interpolation_ms", 200)),
                         keyframes, transforms)
    return anim, tuple(action.get("lt_translation", (0.0, 0.0, 0.0)))


def _bind_animation(nodes, name, dims):
    """One key at the bind pose, for a model with no actions."""
    parents = abc.Model(root=nodes[0]).node_parents()
    tr = []
    for i, n in enumerate(nodes):
        m = Matrix((n.bind_matrix[0:4], n.bind_matrix[4:8], n.bind_matrix[8:12], n.bind_matrix[12:16]))
        if parents[i] >= 0:
            p = nodes[parents[i]].bind_matrix
            m = Matrix((p[0:4], p[4:8], p[8:12], p[12:16])).inverted() @ m
        q = m.to_quaternion()
        tr.append([((m[0][3], m[1][3], m[2][3]), (q.x, q.y, q.z, q.w))])
    return abc.Animation(name, dims, -1, 200, [abc.Keyframe(0, "")], tr)


def save(context, filepath, *, armature=None, scale=1.0, use_selection=False,
         export_animations=True, normalize_weights=True, export_textures=False,
         texture_bpp=None):
    arm_obj = armature or find_armature(context)
    if arm_obj is None:
        raise ExportError(nothing_to_export(context))
    if context.object and context.object.mode != 'OBJECT':
        bpy.ops.object.mode_set(mode='OBJECT')

    fps = context.scene.render.fps / context.scene.render.fps_base
    nodes, node_bones, root = _build_skeleton(arm_obj, scale)
    node_index = {}
    for i, b in enumerate(node_bones):
        if b is not None:
            node_index[b.name] = i
    node_binv = [Matrix((n.bind_matrix[0:4], n.bind_matrix[4:8], n.bind_matrix[8:12],
                         n.bind_matrix[12:16])).inverted_safe() for n in nodes]

    m = abc.Model(version=int(arm_obj.get("lt_version", 12)), root=root)
    m.command_string = arm_obj.get("lt_command_string", "")
    m.lod_distances = [float(d) for d in arm_obj.get("lt_lod_distances", [])]
    if "lt_header_reserved" in arm_obj:
        m.header_reserved = bytes(list(arm_obj["lt_header_reserved"]))

    # Pieces.
    candidates = context.selected_objects if use_selection else arm_obj.children_recursive
    if not use_selection:
        candidates = set(candidates) | {o for o in context.scene.objects
                                        if any(md.type == 'ARMATURE' and md.object == arm_obj
                                               for md in getattr(o, "modifiers", ()))}
    meshes = _mesh_objects(arm_obj, candidates)
    depsgraph = context.evaluated_depsgraph_get()
    arm_inv = arm_obj.matrix_world.inverted()

    base = sorted((o for o in meshes if int(o.get("lt_lod", 0)) == 0),
                  key=lambda o: (o.get("lt_piece_index", 1 << 30), o.name))

    def piece_key(o):
        return ("i", o["lt_piece_index"]) if "lt_piece_index" in o \
            else ("n", o.get("lt_piece", o.name))

    extra = {}
    for o in meshes:
        li = int(o.get("lt_lod", 0))
        if li > 0:
            extra.setdefault((piece_key(o), li), []).append(o)

    nlods = 1 + len(m.lod_distances)
    images = {}
    for o in base:
        default_node = 0
        if o.parent_type == 'BONE' and o.parent_bone in node_index:
            default_node = node_index[o.parent_bone]
        piece_name = o.get("lt_piece", o.name)

        def lods_of(obj):
            me = _evaluated_mesh(obj, depsgraph)
            try:
                return _lods_from_object(obj, me, _to_armature(obj, arm_obj, arm_inv), scale,
                                         node_index, node_binv, default_node,
                                         normalize_weights)
            finally:
                bpy.data.meshes.remove(me)

        by_mat = lods_of(o)
        lod_sets = [by_mat]
        for li in range(1, nlods):
            objs = extra.get((piece_key(o), li)) or extra.get((("n", piece_name), li))
            lod_sets.append(lods_of(objs[0]) if objs else None)

        multi = len(by_mat) > 1
        for mi in sorted(by_mat):
            mat = o.material_slots[mi].material if mi < len(o.material_slots) else None
            if "lt_texture_index" in o:
                tex_index = int(o["lt_texture_index"])
            elif mat is not None and "lt_texture_index" in mat:
                tex_index = int(mat["lt_texture_index"])
            else:
                tex_index = mi
            if export_textures and tex_index not in images:
                img = textures.material_image(mat)
                if img is not None:
                    images[tex_index] = img
            p = abc.Piece(piece_name if not multi else "%s_%s" % (piece_name, mat.name if mat else mi),
                          tex_index,
                          float(o.get("lt_specular_power", 5.0)),
                          float(o.get("lt_specular_scale", 1.0)),
                          float(o.get("lt_lod_weight", 1.0)),
                          list(o.get("lt_reserved", [])))
            for li, ls in enumerate(lod_sets):
                lod = ls.get(mi) if ls else None
                p.lods.append(lod if lod is not None else by_mat[mi])
            m.pieces.append(p)
    if not m.pieces and not export_animations:
        raise ExportError("no meshes found for armature %r" % arm_obj.name)

    lt_points = [v.location for p in m.pieces for v in p.lods[0].vertices]
    # What the tools put here isn't the furthest vertex (ABC.md); keep the
    # imported value, else use that.
    m.internal_radius = float(arm_obj["lt_internal_radius"]) if "lt_internal_radius" in arm_obj \
        else max((Vector(v).length for v in lt_points), default=0.0)

    # Sockets.
    for o in sorted(arm_obj.children, key=lambda o: (o.get("lt_index", 1 << 30), o.name)):
        if o.type != 'EMPTY' or o.parent_type != 'BONE' or o.parent_bone not in node_index:
            continue
        if use_selection and not o.select_get():
            continue
        bone = arm_obj.data.bones[o.parent_bone]
        local_b = (convert.bone_correction(bone).inverted() @
                   Matrix.Translation((0.0, bone.length, 0.0)) @ o.matrix_parent_inverse @ o.matrix_basis)
        loc, rot, _ = local_b.decompose()
        local_b = Matrix.Translation(loc) @ rot.to_matrix().to_4x4()
        pos, q = convert.transform_from_blender(local_b, scale)
        m.sockets.append(abc.Socket(node_index[o.parent_bone], _lt_name(o, o.name), q, pos))

    # Weight sets.
    for ws in json.loads(arm_obj.get("lt_weight_sets_json", "[]")):
        w = ws.get("weights", {})
        m.weight_sets.append(abc.WeightSet(ws["name"], [float(w.get(n.name, 0.0)) for n in nodes]))

    # Animations.
    rest_locals = []
    for i, b in enumerate(node_bones):
        if b is None:
            rest_locals.append(Matrix.Identity(4))
        elif b.parent is None:
            rest_locals.append(b.matrix_local.copy())
        else:
            rest_locals.append(b.parent.matrix_local.inverted() @ b.matrix_local)
    translations = []
    if export_animations:
        for a in _actions_for(arm_obj, node_bones):
            anim, tr = _animation_from_action(a, arm_obj, nodes, node_bones, rest_locals,
                                              scale, fps)
            m.animations.append(anim)
            translations.append(tr)
    if not m.animations:
        if not lt_points:
            raise ExportError("nothing to export: no meshes and no actions")
        lo = [min(v[i] for v in lt_points) for i in range(3)]
        hi = [max(v[i] for v in lt_points) for i in range(3)]
        dims = tuple(float(round(max(abs(lo[i]), abs(hi[i])))) for i in range(3))
        m.animations.append(_bind_animation(nodes, "base", dims))
        translations.append((0.0, 0.0, 0.0))

    # Child models: the model itself first, then those it was imported with.
    stored = json.loads(arm_obj.get("lt_child_models_json", "[]"))
    self_entry = {"name": "", "save_index": int(arm_obj.get("lt_save_index", 1)),
                  "relation": json.loads(arm_obj.get("lt_relation_json", "{}"))}
    ident = ((0.0, 0.0, 0.0), (0.0, 0.0, 0.0, 1.0))
    for ci, c in enumerate([self_entry] + stored):
        rel = c.get("relation", {})
        relation = [(tuple(rel[n.name][0]), tuple(rel[n.name][1])) if n.name in rel else ident
                    for n in nodes]
        m.child_models.append(abc.ChildModel(c["name"], int(c["save_index"]), relation))
        if ci == 0:
            m.anim_bindings.append([abc.AnimBinding(a.name, a.dims, tr)
                                    for a, tr in zip(m.animations, translations)])
        else:
            m.anim_bindings.append([abc.AnimBinding(b[0], tuple(b[1]), tuple(b[2]))
                                    for b in c.get("anim_bindings", [])])

    abc.write_file(m, filepath)

    written = [filepath]
    if export_textures:
        d = os.path.dirname(filepath)
        for ti, img in sorted(images.items()):
            stem = os.path.splitext(img.name)[0]
            path = os.path.join(d, stem + ".dtx")
            textures.save_image(img, path, bpp=texture_bpp)
            written.append(path)
    return m, written

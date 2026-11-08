"""Edits .abc v6 models in Blender and checks what export_abc6 writes.

    blender -b --factory-startup --python tests/blender_abc6_edit.py -- \\
        --va <v6 model with vertex animation> --plain <v6 model without> [--out DIR]

Each case imports a model, makes one edit, exports v6, and checks two
things:

  * WHAT CHANGED: the file read back against the original, field by field.
    Only what the edit touches may differ (and bounds, which are worked out
    again when what they bound has changed).
  * WHAT BLENDER SHOWS: at every keyframe of the edited animation, the mesh
    Blender evaluates (armature and shape keys) against the exported file
    posed by the shared model's own functions (abc6.to_abc,
    abc.pose_matrices, abc.animated_vertices), every vertex: within TOL_POS,
    or within a vertex-animation frame's quantization for the vertices a
    node animates.

The cases:

  operator    File > Import, then File > Export with its default, "As
              Imported": the v6 file back byte for byte; and Version 12
              still writes a v12 file.
  va-nudge    one animated vertex moved 1 unit in one frame's shape key:
              that frame's three bytes change, nothing else in the tracks.
  va-far      the same vertex moved 1000 units: no longer fits the stored
              scale, so the node's frames get a new scale and translation.
  bone        a bone turned 10 degrees at one key of an animation that is
              not the first: that key alone changes, written unit length.
  vertex      a vertex of the plain model moved: its floats change, the
              reduced-detail vertices stay (they refer to it by number).
  face        a face of the plain model deleted: the reduced-detail
              vertices are dropped, NumLODs 0, replacements 0.
  new-va      vertex animation made in Blender on the plain model: shape
              keys moving one vertex in one animation; its node gains flag
              4 and lists every vertex on it, as shipped deformation nodes
              do.

With --out the edited files are kept there, for the engine
(Lithtech_Disassembly's tools/rezview/pose_check.py) to pose.
Exits non-zero if a case fails.
"""

import math
import os
import shutil
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
sys.path.insert(0, str(HERE))

import bpy  # noqa: E402
from mathutils import Quaternion, Vector  # noqa: E402
import io_scene_lithtech  # noqa: E402
from io_scene_lithtech import animation, convert, export_abc6, import_abc  # noqa: E402
from io_scene_lithtech.lithtech import abc, abc6  # noqa: E402
import blender_roundtrip  # noqa: E402
from blender_abc6 import blender_positions  # noqa: E402

TOL_POS = 2e-3


def load(path):
    blender_roundtrip.purge()
    arm = import_abc.load(bpy.context, str(path), import_textures=False, child_animations=False,
                          import_lods=False, import_sockets=False)
    obj = next(o for o in arm.children if o.type == 'MESH')
    return arm, obj


def actions_of(arm):
    return sorted((a for a in bpy.data.actions if a.get("lt_model") == arm.name), key=lambda a: a["lt_index"])


def export(arm, out):
    animation.select_action(bpy.context, arm, None)
    _, _, notes = export_abc6.save(bpy.context, str(out), armature=arm)
    return abc6.read(Path(out).read_bytes()), notes


def pose_against_file(arm, obj, act, path, shape_values=None):
    """Worst distance, at every keyframe of act, between Blender's mesh and
    the exported file posed by the shared model: (skeletal, animated)."""
    sm = abc6.to_abc(abc6.read(Path(path).read_bytes()))
    lod = sm.pieces[0].lods[0]
    ai = act["lt_index"]
    anim = sm.animations[ai]
    va = {v for n in sm.nodes for v in n.animated_vertices}
    fps = bpy.context.scene.render.fps / bpy.context.scene.render.fps_base
    animation.select_action(bpy.context, arm, act)
    worst_s = worst_v = 0.0
    for k, kf in enumerate(anim.keyframes):
        if shape_values is not None:
            shape_values(k)
        got = blender_positions(arm, obj, kf.time * fps / 1000.0)
        M = abc.pose_matrices(sm, anim, k)
        want = [abc.skinned_position(sm, v, M) for v in abc.animated_vertices(sm, anim, k, lod)]
        for i, (g, w) in enumerate(zip(got, want)):
            d = max(abs(g[c] - w[c]) for c in range(3))
            if i in va:
                worst_v = max(worst_v, d)
            else:
                worst_s = max(worst_s, d)
    animation.select_action(bpy.context, arm, None)
    return worst_s, worst_v


def diff_fields(a, b):
    """The parts of two v6 models that differ, as names."""
    out = []
    for field in ("chunks", "command_string", "vertex_start", "normal_vertex_count", "anim_dims"):
        if getattr(a, field) != getattr(b, field):
            out.append(field)
    if (a.bounds_min, a.bounds_max) != (b.bounds_min, b.bounds_max):
        out.append("bounds")
    for name in ("vertices", "triangles"):
        la, lb = getattr(a, name), getattr(b, name)
        if len(la) != len(lb):
            out.append("%s count" % name)
        else:
            out += ["%s %d" % ({"vertices": "vertex"}.get(name, name[:-1]), i)
                    for i, (x, y) in enumerate(zip(la, lb)) if x != y]
    for i, (x, y) in enumerate(zip(a.nodes, b.nodes)):
        if (x.bounds_min, x.bounds_max) != (y.bounds_min, y.bounds_max):
            out.append("node %d bounds" % i)
        for field in ("name", "index", "flags", "animated_vertices", "child_count"):
            if getattr(x, field) != getattr(y, field):
                out.append("node %d %s" % (i, field))
    for ai, (x, y) in enumerate(zip(a.animations, b.animations)):
        if (x.name, x.length) != (y.name, y.length):
            out.append("animation %d header" % ai)
        if (x.bounds_min, x.bounds_max) != (y.bounds_min, y.bounds_max) or \
                [(k.bounds_min, k.bounds_max) for k in x.keyframes] != [(k.bounds_min, k.bounds_max) for k in y.keyframes]:
            out.append("animation %d bounds" % ai)
        if [(k.time, k.command) for k in x.keyframes] != [(k.time, k.command) for k in y.keyframes]:
            out.append("animation %d keyframes" % ai)
        for ni, (tx, ty) in enumerate(zip(x.tracks, y.tracks)):
            for k, (kx, ky) in enumerate(zip(tx.keys, ty.keys)):
                if kx != ky:
                    out.append("animation %d node %d key %d" % (ai, ni, k))
            if (tx.scale, tx.translation) != (ty.scale, ty.translation):
                out.append("animation %d node %d scale" % (ai, ni))
            if tx.vertex_bytes != ty.vertex_bytes:
                nb = sum(p != q for p, q in zip(tx.vertex_bytes, ty.vertex_bytes)) + \
                    abs(len(tx.vertex_bytes) - len(ty.vertex_bytes))
                out.append("animation %d node %d bytes (%d)" % (ai, ni, nb))
    if len(a.animations) != len(b.animations):
        out.append("animation count")
    return out


def only(changed, allowed):
    """The changes not in allowed (prefixes)."""
    return [c for c in changed if not any(c == p or c.startswith(p + " ") for p in allowed)]


def va_vertex(orig):
    """A node with animated vertices, its first vertex that no other entry
    repeats, and an action that has frames for it."""
    for i, n in enumerate(orig.nodes):
        for j, v in enumerate(n.animated_vertices):
            if n.animated_vertices.count(v) == 1:
                return i, j, v
    raise RuntimeError("no animated vertex")


def main(argv):
    args = argv[argv.index("--") + 1:] if "--" in argv else []
    va_path = plain_path = out_dir = None
    i = 0
    while i < len(args):
        if args[i] == "--va":
            va_path = Path(args[i + 1]); i += 2
        elif args[i] == "--plain":
            plain_path = Path(args[i + 1]); i += 2
        elif args[i] == "--out":
            out_dir = Path(args[i + 1]); i += 2
        else:
            raise SystemExit("unknown argument %r" % args[i])
    io_scene_lithtech.register()
    tmp = Path(tempfile.mkdtemp(prefix="lt_abc6_edit_"))
    if out_dir:
        out_dir.mkdir(parents=True, exist_ok=True)
    results = []

    def report(case, problems, detail):
        results.append((case, problems))
        print("%-9s %s  %s" % (case, "FAIL" if problems else "ok", detail))
        for p in problems[:6]:
            print("            " + p)

    def keep(path, case):
        if out_dir:
            shutil.copy(path, out_dir / ("%s_%s" % (case, path.name)))

    if va_path:
        orig = abc6.read(va_path.read_bytes())
        ni, j, v = va_vertex(orig)
        for case, dist in (("va-nudge", 1.0), ("va-far", 1000.0)):
            arm, obj = load(va_path)
            acts = actions_of(arm)
            ai = 1 if len(acts) > 1 else 0
            act, k = acts[ai], min(1, len(orig.animations[ai].keyframes) - 1)
            kb = obj.data.shape_keys.key_blocks[import_abc.shape_key_name(act.name, k)]
            kb.data[v].co = kb.data[v].co + convert.vec_to_blender((dist, 0.0, 0.0))
            out = tmp / va_path.name
            got, notes = export(arm, out)
            keep(out, case)
            changed = diff_fields(got, orig)
            allowed = ["animation %d node %d bytes" % (ai, ni), "animation %d bounds" % ai]
            if case == "va-far":
                allowed.append("animation %d node %d scale" % (ai, ni))
            problems = ["unexpected: " + c for c in only(changed, allowed)]
            if not any(c.startswith("animation %d node %d bytes" % (ai, ni)) for c in changed):
                problems.append("the frame did not change")
            sk, svv = pose_against_file(arm, obj, act, out)
            tr = got.animations[ai].tracks[ni]
            quant = max(tr.scale) * 0.87 + 1e-3   # half a step on each axis, rotated
            if sk > TOL_POS:
                problems.append("skeletal vertices off by %g" % sk)
            if svv > quant:
                problems.append("animated vertices off by %g (quantisation %g)" % (svv, quant))
            if case == "va-nudge":
                nb = [c for c in changed if c.startswith("animation %d node %d bytes" % (ai, ni))]
                if nb != ["animation %d node %d bytes (%d)" % (ai, ni, int(nb[0].split("(")[1][:-1]))] or \
                        int(nb[0].split("(")[1][:-1]) > 3:
                    problems.append("more than one entry's bytes changed: %s" % nb)
            report(case, problems, "%s, animation %d key %d, vertex %d on node %d: %s; Blender against the file "
                   "%.2g skeletal, %.2g animated (step %.3g)"
                   % (va_path.name, ai, k, v, ni, ", ".join(changed) or "nothing changed", sk, svv, max(tr.scale)))

    if va_path:
        # The operators, as the File menu runs them: "As Imported" is v6.
        blender_roundtrip.purge()
        bpy.ops.import_scene.lithtech_abc(filepath=str(va_path), import_textures=False)
        out = tmp / ("op_" + va_path.name)
        res = bpy.ops.export_scene.lithtech_abc(filepath=str(out))
        same = out.exists() and out.read_bytes() == va_path.read_bytes()
        res12 = bpy.ops.export_scene.lithtech_abc(filepath=str(tmp / "op12.abc"), abc_version='12')
        v12 = (tmp / "op12.abc").exists() and not abc6.is_model((tmp / "op12.abc").read_bytes())
        report("operator", [] if same and v12 and res == {'FINISHED'} and res12 == {'FINISHED'} else
               ["export %s, identical %s; version 12 %s, written as v12 %s" % (res, same, res12, v12)],
               "File > Import then File > Export of %s: As Imported writes it back byte for byte (%s); "
               "Version 12 still writes v12 (%s)" % (va_path.name, same, v12))

    if plain_path:
        orig = abc6.read(plain_path.read_bytes())
        # bone
        arm, obj = load(plain_path)
        acts = actions_of(arm)
        ai = 1 if len(acts) > 1 else 0
        act = acts[ai]
        k = min(1, len(orig.animations[ai].keyframes) - 1)
        bone = arm.data.bones[min(3, len(arm.data.bones) - 1)]
        node = bone["lt_index"]
        fps = bpy.context.scene.render.fps / bpy.context.scene.render.fps_base
        frame = orig.animations[ai].keyframes[k].time * fps / 1000.0
        path = 'pose.bones["%s"].rotation_quaternion' % bpy.utils.escape_identifier(bone.name)
        fcs = {fc.array_index: fc for fc in export_abc6.export_abc._action_fcurves(act, arm) if fc.data_path == path}
        kps = [min(fcs[c].keyframe_points, key=lambda p: abs(p.co[0] - frame)) for c in range(4)]
        q = Quaternion([p.co[1] for p in kps])
        q = q @ Quaternion((1.0, 0.0, 0.0), math.radians(10.0))
        for c in range(4):
            kps[c].co[1] = q[c]
            fcs[c].update()
        out = tmp / plain_path.name
        got, notes = export(arm, out)
        keep(out, "bone")
        changed = diff_fields(got, orig)
        problems = ["unexpected: " + c for c in only(changed, ["animation %d node %d key %d" % (ai, node, k),
                                                               "animation %d bounds" % ai])]
        if "animation %d node %d key %d" % (ai, node, k) not in changed:
            problems.append("the key did not change")
        qn = math.sqrt(sum(c * c for c in got.animations[ai].tracks[node].keys[k][1]))
        if abs(qn - 1.0) > 1e-5:
            problems.append("the new key is not unit length (%g)" % qn)
        sk, _ = pose_against_file(arm, obj, act, out)
        if sk > TOL_POS:
            problems.append("vertices off by %g" % sk)
        report("bone", problems, "%s, animation %d key %d, node %d: %s; Blender against the file %.2g"
               % (plain_path.name, ai, k, node, ", ".join(changed), sk))

        # vertex
        arm, obj = load(plain_path)
        vi = 0
        obj.data.vertices[vi].co = obj.data.vertices[vi].co + Vector((0.0, 0.0, 1.0))
        got, notes = export(arm, out)
        keep(out, "vertex")
        changed = diff_fields(got, orig)
        # Blender keeps custom normals relative to their faces, so moving a
        # vertex turns its neighbors' corner normals too, and those
        # vertices take Blender's normals: allowed, position unchanged. The
        # faces around it get new face normals.
        near = {i for t in orig.triangles if vi in t.indices for i in t.indices}
        problems = ["unexpected: " + c for c in only(changed, ["vertex %d" % i for i in near] +
                                                     ["triangle %d" % i for i, t in enumerate(orig.triangles)
                                                      if vi in t.indices] + ["bounds", "node", "animation"])]
        if any(c.startswith("animation") and "bounds" not in c for c in changed):
            problems.append("animations changed: %s" % [c for c in changed if c.startswith("animation")][:3])
        if any(c.startswith("node") and "bounds" not in c for c in changed):
            problems.append("nodes changed")
        if "vertex %d" % vi not in changed:
            problems.append("the vertex did not change")
        for i in near - {vi}:
            if got.vertices[i].position != orig.vertices[i].position:
                problems.append("neighbour %d moved" % i)
        for i, t in enumerate(orig.triangles):
            if vi in t.indices and (got.triangles[i].uvs, got.triangles[i].indices) != (t.uvs, t.indices):
                problems.append("triangle %d changed beyond its normal" % i)
        if got.vertex_start != orig.vertex_start or len(got.vertices) != len(orig.vertices):
            problems.append("the reduced-detail vertices were not kept")
        sk, _ = pose_against_file(arm, obj, actions_of(arm)[0], out)
        if sk > TOL_POS:
            problems.append("vertices off by %g" % sk)
        report("vertex", problems, "%s, vertex %d up 1: %s; reduced-detail vertices %d of %d kept; "
               "Blender against the file %.2g"
               % (plain_path.name, vi, ", ".join(c for c in changed if "bounds" not in c) + ", bounds",
                  len(got.vertices) - got.normal_vertex_count, len(orig.vertices) - orig.normal_vertex_count, sk))

        # face
        arm, obj = load(plain_path)
        import bmesh
        bm = bmesh.new()
        bm.from_mesh(obj.data)
        bm.faces.ensure_lookup_table()
        bmesh.ops.delete(bm, geom=[bm.faces[0]], context='FACES_ONLY')
        bm.to_mesh(obj.data)
        bm.free()
        got, notes = export(arm, out)
        keep(out, "face")
        problems = []
        if len(got.triangles) != len(orig.triangles) - 1:
            problems.append("%d triangles for %d" % (len(got.triangles), len(orig.triangles) - 1))
        if got.triangles != orig.triangles[1:]:
            problems.append("the other triangles changed")
        if got.vertex_start != [0] or len(got.vertices) != got.normal_vertex_count:
            problems.append("the reduced-detail vertices were kept")
        if any(v.replacements != (0, 0) for v in got.vertices):
            problems.append("replacements kept")
        if [v.position for v in got.vertices] != [v.position for v in orig.vertices[:orig.normal_vertex_count]]:
            problems.append("vertex positions changed")
        sk, _ = pose_against_file(arm, obj, actions_of(arm)[0], out)
        if sk > TOL_POS:
            problems.append("vertices off by %g" % sk)
        report("face", problems, "%s, face 0 deleted: %d triangles, NumLODs %d, notes: %s; Blender against "
               "the file %.2g" % (plain_path.name, len(got.triangles), len(got.vertex_start) - 1,
                                   "; ".join(notes) or "-", sk))

        # new-va
        arm, obj = load(plain_path)
        acts = actions_of(arm)
        ai = 1 if len(acts) > 1 else 0
        act = acts[ai]
        nk = len(orig.animations[ai].keyframes)
        obj.shape_key_add(name="Basis", from_mix=False)
        vi = 5
        names = []
        for k in range(nk):
            kb = obj.shape_key_add(name=import_abc.shape_key_name(act.name, k), from_mix=False)
            kb.value = 0.0
            # Rising to 4 units at the last keyframe.
            lift = 4.0 * k / max(1, nk - 1)
            kb.data[vi].co = kb.data[vi].co + convert.vec_to_blender((0.0, lift, 0.0))
            names.append(kb.name)
        node_of = orig.vertices[vi].node
        got, notes = export(arm, out)
        keep(out, "new-va")
        changed = diff_fields(got, orig)
        problems = ["unexpected: " + c for c in only(changed, ["node %d flags" % node_of,
                                                               "node %d animated_vertices" % node_of,
                                                               "animation"])]
        own = [v for v in range(orig.normal_vertex_count) if orig.vertices[v].node == node_of]
        if got.nodes[node_of].animated_vertices != own:
            problems.append("node %d animates %r, not its %d vertices" % (node_of, got.nodes[node_of].animated_vertices[:8], len(own)))
        if not got.nodes[node_of].flags & abc6.NODE_DEFORMATION:
            problems.append("no deformation flag")
        keys_changed = [c for c in changed if " key " in c]
        if keys_changed:
            problems.append("bone keys changed: %s" % keys_changed[:3])

        def values(k):
            for i, n in enumerate(names):
                obj.data.shape_keys.key_blocks[n].value = 1.0 if i == k else 0.0
        sk, svv = pose_against_file(arm, obj, act, out, values)
        tr = got.animations[ai].tracks[node_of]
        quant = max(tr.scale) * 0.87 + 1e-3
        if sk > TOL_POS:
            problems.append("skeletal vertices off by %g" % sk)
        if svv > quant:
            problems.append("the animated vertex off by %g (quantisation %g)" % (svv, quant))
        report("new-va", problems, "%s, vertex %d lifted in animation %d: node %d flags %d, animates its %d "
               "vertices; Blender against the file %.2g skeletal, %.2g animated (step %.3g)"
               % (plain_path.name, vi, ai, node_of, got.nodes[node_of].flags,
                  len(got.nodes[node_of].animated_vertices), sk, svv, max(tr.scale)))

    failed = sum(1 for _, p in results if p)
    print("%d cases, %d failed" % (len(results), failed))
    return 1 if failed or not results else 0


if __name__ == "__main__":
    code = main(sys.argv)
    sys.stdout.flush()
    os._exit(code)

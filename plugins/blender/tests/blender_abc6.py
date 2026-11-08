"""Imports each .abc v6 (Shogo, Blood 2, KISS) into Blender, checks the pose
the armature and the shape keys give against the shared model's, and
exports it again, as v12 and as v6.

    blender -b --factory-startup --python tests/blender_abc6.py -- \
        [--limit N] [--every K] [--keys N] <paths or directories...>

For each model:

  * POSE: at every keyframe (up to --keys per animation, spread out), the
    mesh Blender evaluates through the armature, brought back to LithTech
    space, against abc.skinned_position on the shared model abc6.to_abc
    gives (abc.pose_matrices, and abc.animated_vertices for the vertices a
    node animates), every vertex. Within TOL_POS. The vertices a node
    animates are also reported apart: they get there through the shape
    keys import_abc makes of their frames.
  * MIDPOINTS: the same halfway between keys, MEASURED and not asserted:
    Blender lerps quaternion components where the engine slerps, which is
    the same at keys and close between them.
  * EXPORT: export_abc.save writes the armature as an .abc v12, which must
    compare with the shared model as tests/blender_roundtrip.py compares a
    v12 file with its export (v12 has no vertex animation, so the frames
    are not in it).
  * EXPORT v6: export_abc6.save writes it as LithTech 1's .abc v6, which
    must be the file imported, byte for byte.

Exits non-zero if any model fails.
"""

import math
import os
import sys
import tempfile
import time
import traceback
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
sys.path.insert(0, str(HERE))

import bpy  # noqa: E402
from mathutils import Vector  # noqa: E402
import io_scene_lithtech  # noqa: E402
from io_scene_lithtech import animation, convert, export_abc, export_abc6, import_abc  # noqa: E402
from io_scene_lithtech.lithtech import abc, abc6  # noqa: E402
import blender_roundtrip  # noqa: E402

TOL_POS = 2e-3


def purge():
    blender_roundtrip.purge()


def posed_reference(m, anim, k, lod):
    M = abc.pose_matrices(m, anim, k)
    return [abc.skinned_position(m, v, M) for v in abc.animated_vertices(m, anim, k, lod)]


def mid_reference(m, anim, k, lod):
    """Halfway between keys k and k+1: each node's pose lerped and slerped,
    as the engine does, then each vertex skinned; an animated vertex's
    position lerped."""
    parents = m.node_parents()
    out = []
    for i, per in enumerate(anim.node_transforms):
        (p0, q0), (p1, q1) = per[k], per[k + 1]
        p = tuple(a + (b - a) * 0.5 for a, b in zip(p0, p1))
        d = sum(a * b for a, b in zip(q0, q1))
        if d < 0:
            q1, d = tuple(-c for c in q1), -d
        if d < 0.9995:
            th = math.acos(min(1.0, d))
            s0, s1 = math.sin(0.5 * th) / math.sin(th), math.sin(0.5 * th) / math.sin(th)
        else:
            s0 = s1 = 0.5
        q = tuple(s0 * a + s1 * b for a, b in zip(q0, q1))
        local = abc.quat_to_matrix(p, q)
        out.append(local if parents[i] < 0 else abc.matrix_multiply(out[parents[i]], local))
    verts = list(lod.vertices)
    if anim.vertex_frames:
        for i, node in enumerate(m.nodes):
            if node.animated_vertices and anim.vertex_frames[i]:
                f0, f1 = anim.vertex_frames[i][k], anim.vertex_frames[i][k + 1]
                for j, vi in enumerate(node.animated_vertices):
                    p = tuple(a + (b - a) * 0.5 for a, b in zip(f0[j], f1[j]))
                    v = verts[vi]
                    verts[vi] = abc.Vertex([abc.Weight(w.node, p, w.bias) for w in v.weights],
                                           v.replacement, v.location, v.normal)
    return [abc.skinned_position(m, v, out) for v in verts]


def blender_positions(arm, obj, frame):
    bpy.context.scene.frame_set(math.floor(frame), subframe=frame % 1.0)
    bpy.context.view_layer.update()
    dg = bpy.context.evaluated_depsgraph_get()
    ev = obj.evaluated_get(dg)
    me = ev.to_mesh()
    to_arm = arm.matrix_world.inverted() @ obj.matrix_world
    scale = arm.get("lt_scale", 1.0) or 1.0
    out = [convert.vec_from_blender(to_arm @ v.co, 1.0 / scale) for v in me.vertices]
    ev.to_mesh_clear()
    return out


def first_difference(a, b):
    """Where two v6 models first differ, for the report."""
    for field in ("chunks", "command_string", "bounds_min", "bounds_max", "vertex_start",
                  "normal_vertex_count", "anim_dims"):
        if getattr(a, field) != getattr(b, field):
            return field
    for name in ("triangles", "vertices", "nodes"):
        la, lb = getattr(a, name), getattr(b, name)
        if len(la) != len(lb):
            return "%s: %d for %d" % (name, len(la), len(lb))
        for i, (x, y) in enumerate(zip(la, lb)):
            if x != y:
                return "%s %d: %r for %r" % (name, i, x, y)
    for ai, (x, y) in enumerate(zip(a.animations, b.animations)):
        for field in ("name", "length", "bounds_min", "bounds_max", "keyframes"):
            if getattr(x, field) != getattr(y, field):
                return "animation %d %s" % (ai, field)
        for ni, (tx, ty) in enumerate(zip(x.tracks, y.tracks)):
            for k, (kx, ky) in enumerate(zip(tx.keys, ty.keys)):
                if kx != ky:
                    return "animation %d node %d key %d: %r for %r" % (ai, ni, k, kx, ky)
            if tx != ty:
                return "animation %d node %d vertex frames" % (ai, ni)
    return "animations: %d for %d" % (len(a.animations), len(b.animations))


def worst(got, want, skip):
    return max((max(abs(g[c] - w[c]) for c in range(3))
                for i, (g, w) in enumerate(zip(got, want)) if i not in skip), default=0.0)


def main(argv):
    args = argv[argv.index("--") + 1:] if "--" in argv else []
    limit = every = None
    keys_per = 4
    paths = []
    i = 0
    while i < len(args):
        if args[i] == "--limit":
            limit = int(args[i + 1]); i += 2
        elif args[i] == "--every":
            every = int(args[i + 1]); i += 2
        elif args[i] == "--keys":
            keys_per = int(args[i + 1]); i += 2
        else:
            paths.append(Path(args[i])); i += 1
    files = []
    for p in paths:
        files += sorted(f for f in p.rglob("*") if f.suffix.lower() == ".abc") if p.is_dir() else [p]
    files = [f for f in files if abc6.is_model(f.read_bytes())]
    if every:
        files = files[::every]
    if limit:
        files = files[:limit]

    io_scene_lithtech.register()
    fps = bpy.context.scene.render.fps / bpy.context.scene.render.fps_base
    tmp = Path(tempfile.mkdtemp(prefix="lt_abc6_"))
    failed = poses = mids = v6_same = 0
    worst_key = worst_mid = worst_va = worst_va_mid = 0.0
    where_mid = ""
    unit_mid = 0.0    # the worst midpoint between two unit-length keys
    va_models = 0
    export_worst = {}
    t0 = time.time()
    for n, f in enumerate(files, 1):
        purge()
        try:
            m = abc6.to_abc(abc6.read(f.read_bytes()))
            lod = m.pieces[0].lods[0]
            arm = import_abc.load(bpy.context, str(f), import_textures=False, child_animations=False,
                                  import_lods=False, import_sockets=False)
            obj = next(o for o in arm.children if o.type == 'MESH')
            if len(obj.data.vertices) != len(lod.vertices):
                raise RuntimeError("%d mesh vertices for %d" % (len(obj.data.vertices), len(lod.vertices)))
            va = {vi for node in m.nodes for vi in node.animated_vertices}
            va_models += bool(va)
            actions = {a.get("lt_index"): a for a in bpy.data.actions if a.get("lt_model") == arm.name}
            problems = []
            for ai, anim in enumerate(m.animations):
                act = actions.get(ai)
                if act is None or not anim.keyframes:
                    problems.append("no action for %s" % anim.name)
                    continue
                animation.select_action(bpy.context, arm, act)
                nk = len(anim.keyframes)
                ks = sorted({round(j * (nk - 1) / max(1, keys_per - 1)) for j in range(min(nk, keys_per))})
                for k in ks:
                    got = blender_positions(arm, obj, anim.keyframes[k].time * fps / 1000.0)
                    want = posed_reference(m, anim, k, lod)
                    d = worst(got, want, ())
                    poses += 1
                    worst_key = max(worst_key, d)
                    if d > TOL_POS:
                        problems.append("%s key %d: off by %g" % (anim.name, k, d))
                    if va:
                        worst_va = max(worst_va, max(max(abs(got[i][c] - want[i][c]) for c in range(3)) for i in va))
                    if k + 1 < nk and anim.keyframes[k + 1].time > anim.keyframes[k].time:
                        t = (anim.keyframes[k].time + anim.keyframes[k + 1].time) / 2.0
                        got = blender_positions(arm, obj, t * fps / 1000.0)
                        mref = mid_reference(m, anim, k, lod)
                        dm = worst(got, mref, va)
                        if va:
                            worst_va_mid = max(worst_va_mid, max(max(abs(got[i][c] - mref[i][c]) for c in range(3))
                                                                 for i in va))
                        if dm > worst_mid:
                            worst_mid, where_mid = dm, "%s %s after key %d" % (f.name, anim.name, k)
                        unit = all(abs(math.sqrt(sum(c * c for c in per[j][1])) - 1.0) < 1e-3
                                   for per in anim.node_transforms for j in (k, k + 1))
                        if unit:
                            unit_mid = max(unit_mid, dm)
                        mids += 1
            animation.select_action(bpy.context, arm, None)
            out = tmp / "out.abc"
            export_abc.save(bpy.context, str(out), armature=arm, normalize_weights=False)
            d = blender_roundtrip.compare(m, abc.read_file(out))
            for key, v in d.max.items():
                if v[0] > export_worst.get(key, (0.0,))[0]:
                    export_worst[key] = (v[0], "%s: %s" % (f.name, v[1]))
            problems += ["export: " + e for e in d.errors]
            out6 = tmp / "out6.abc"
            export_abc6.save(bpy.context, str(out6), armature=arm)
            if out6.read_bytes() == f.read_bytes():
                v6_same += 1
            else:
                problems.append("v6 export differs: " + first_difference(abc6.read(out6.read_bytes()),
                                                                         abc6.read(f.read_bytes())))
        except Exception:
            failed += 1
            print("FAIL %s: exception" % f)
            traceback.print_exc()
            continue
        if problems:
            failed += 1
            print("FAIL %s: %d problems; first: %s" % (f, len(problems), "; ".join(problems[:3])))
        if n % 25 == 0:
            print("... %d/%d, %d failed, %.0fs" % (n, len(files), failed, time.time() - t0))
    print()
    print("export, worst differences:")
    for key in sorted(export_worst):
        print("  %-11s %-10.3g %s" % (key, export_worst[key][0], export_worst[key][1]))
    print("%d v6 models, %d failed: %d keyframe poses (worst %.3g), %d midpoints measured (worst %.3g, %s; "
          "%.3g where both keys are unit length); %d models with vertex animation (those vertices worst %.3g "
          "at keys, %.3g halfway); v6 export identical for %d of %d; %.0fs"
          % (len(files), failed, poses, worst_key, mids, worst_mid, where_mid or "-", unit_mid, va_models,
             worst_va, worst_va_mid, v6_same, len(files), time.time() - t0))
    return 1 if failed or not files else 0


if __name__ == "__main__":
    code = main(sys.argv)
    sys.stdout.flush()
    os._exit(code)

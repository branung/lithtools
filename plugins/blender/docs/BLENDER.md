# The Blender extension

`io_scene_lithtech` imports and exports LithTech models (`.abc` versions 9
to 12: NOLF, AvP2; `.abc` version 6: Shogo, Blood 2, KISS Psycho Circus;
`.ltb` model file versions 18 to 25: NOLF2, Contract J.A.C.K., Tron 2.0)
and textures (`.dtx`, version -5) in Blender 5.2 LTS and later.

Die Hard: Nakatomi Plaza's wrapped `.ltb` files can also be imported through
the LTB importer (wrapper version 3, model versions 9–13, ABC or D3D body).
Rigid meshes and the two documented skeletal vertex layouts are supported.
These imports use the shared skeleton, texture and animation handling.
Export converts them to normal ABC 12 or PC LTB; the DHNP wrapper and its
version 13 track extensions are not round-tripped. The layout reference is
[StenApp's DHNP research](https://github.com/StenApp/io_scene_lithtech/blob/0969529990ea8620ad525d8fd89e2aeaeff76eef/research/LTB_D3D_MODEL_FILE_DHNP.bt).

## Installing

Build the package and install it (or drag the zip onto Blender):

```
blender --command extension build --source-dir io_scene_lithtech --output-dir dist
blender --command extension install-file -r user_default -e dist/io_scene_lithtech-0.1.0.zip
```

Then use **File > Import / Export > LithTech Model (.abc)** and **LithTech
Texture (.dtx)**, or drop an `.abc` onto the viewport. In the Image Editor,
**Image > Open / Save as LithTech Texture** does the same for textures.

## Models

| LithTech | Blender |
|---|---|
| node tree | armature; one bone per node, at its bind matrix |
| piece | mesh object parented to the armature, with an Armature modifier |
| vertex weights | vertex groups named after the bones |
| UVs (one per triangle corner) | the `UVMap` layer |
| vertex normals | custom normals |
| LODs 1..n | hidden objects `<piece>_LOD<n>` in a `<model> LODs` collection |
| skin slot | material `<model>_skin<n>`, with the DTX if one was found |
| sockets | arrow empties parented to their node's bone |
| animations | actions (fake user), linear keys at the stored times |
| keyframe strings | the action's pose markers |
| child models' animations | actions too, marked `lt_child` (not exported) |

**Space.** LithTech is left-handed with Y up and models face +Z. The
importer swaps Y and Z (which also fixes the handedness and turns the
triangle winding round), so models stand on Blender's Z up and face +Y.
*Face Front* (on by default) turns the armature object 180° to face -Y, as
Blender's front view expects. The exporter writes the model in the
armature's own space, so the object's transform never reaches the file.

**Scale.** LithTech units have no fixed size; a NOLF character is about
100 units tall. Import *Scale* is Blender units per LithTech unit (0.018
gives a 1.8 m character); export *Scale* is the inverse and defaults to
undo whatever the model was imported at.

**Animations.** Select the armature or one of its meshes and open
**Object Properties > LithTech Animations**. Select an animation in the
list to bind its Action and object slot, set the playback range and move
to its first frame. **Play / Pause** and **Stop** control playback;
**Rest Pose** clears the active Action and mutes NLA tracks. Child-model
animations are labelled with their source file. **Register Active Action**
adds an authored Action to this model's list and exports.
Selecting an animation mutes existing NLA tracks so only the selected
Action drives the rig. Switching Actions in Blender's Action Editor remains
available.

**Skeletons.** Bone heads retain the source joints, bone tails point toward
child joints, and terminal nodes use small joint markers (short bones in
Edit Mode). The original bind matrix and Blender's initial rest matrix are
kept on each bone. Export uses the original matrix while the bone is
unchanged; edited bones use their current matrix with the axis correction.

**Skins.** The file names no textures: the game's attribute files pick
them. The importer looks for `SKINS/<model>.dtx` beside the `MODELS`
directory (NOLF and AvP2 keep `CHARS/MODELS/X.abc` with
`CHARS/SKINS/X.dtx`), then `<model>_*.dtx` for the other slots, in the
model's directory and in *Skin Directory*. A wrong match is fixed by
picking another image in the material.

**Alpha.** Skins often use alpha as a mask, or leave it at 0, so the image
keeps it (`Channel Packed`) without making the material see-through.

### What is kept for export

LithTech data with no place in Blender is kept as custom properties, all
named `lt_*`:

- armature object: `lt_version`, `lt_command_string` (the model's command
  string: shadows, `NormalRef` and so on), `lt_internal_radius`,
  `lt_lod_distances`, `lt_scale`, `lt_save_index`, and JSON for the weight
  sets, the child models and their animation bindings;
- bones: `lt_index` (the order nodes are written in), `lt_flags`,
  `lt_bind_source`, `lt_bind_baseline` and `lt_axis_correction`;
- piece objects: `lt_piece`, `lt_piece_index`, `lt_lod`,
  `lt_texture_index`, `lt_specular_power`, `lt_specular_scale`,
  `lt_lod_weight`;
- actions: `lt_dims` (the box), `lt_translation`, `lt_interpolation_ms`,
  `lt_weight_set`, `lt_index`, and the keyframe strings exactly (pose
  marker names stop at 63 bytes and sit on whole frames);
- meshes: `lt_replacement` (each vertex's collapse target), `lt_normal`
  (the stored normals, since Blender keeps custom normals only to about
  1e-3 and can't hold a zero one) and `lt_import_normal` (the corner
  normals at import, so the exporter can tell which ones were edited).
  Vertices whose weights don't follow from their position (some LODs, and
  biases that don't sum to 1 or name a node twice) also keep their stored
  weights while they stay where they were.

Everything has a default, so models made in Blender export as well: any
mesh deformed by the armature is a piece (one per material), vertex groups
named after bones are the weights, empties on bones are sockets, and every
action that animates the bones is an animation, keyed wherever any of its
F-curves has a key. With no actions the model gets one animation, `base`,
at the rest pose.

### Round trip

`tests/blender_roundtrip.py` imports every shipped model (754: NOLF GOTY
and AvP2 Primal Hunt), exports it and compares the two field by field. All
754 pass. Names, counts, flags, indices, triangles, weights, keyframe
times and strings, and the header data match exactly. Numbers match within
float precision (the worst cases are 6e-5 units in animation positions and
5e-5 in bone rotations, from Blender's bone matrices).

The exported file isn't byte-identical to the shipped one: it is rebuilt
from Blender's data, so floats differ in their last bits, and the original
order of each vertex's weights isn't kept (they're written by node).

## Textures

An imported DTX becomes a packed image with its header settings as custom
properties (`lt_bpp`, `lt_user_flags`, `lt_command`, `lt_texture_group`,
`lt_mipmaps_to_use`, `lt_mip_count`, `lt_priority`, `lt_detail_*`, and
sections such as `LIGHTDEFS`). Exporting writes those settings back.
*Format* picks 32-bit (lossless) or DXT1/3/5. The DXT encoder is a simple
one, not the original tools', so re-encoding a shipped DXT texture changes
its bytes slightly (2-3 levels per channel on average). Images must be
powers of two.

Cube maps import as their first face, and export as a flat texture.

The image also keeps the file it came from (`lt_dtx`) and a digest of its
pixels (`lt_pixels`). Exported unchanged with *Format* "As Imported", it
writes that file back byte for byte, so a DXT texture that was only looked
at keeps its exact bytes.

**PS2 textures** (32P: 256 colors, from the PS2 version of NOLF) import
with their palette alpha read as the PS2 draws it (0x80 is opaque; the
*Palette Alpha* import option is Auto, PS2 or Stored) and export back to
32P (*Format* "32P (PS2)" turns any image into one). An edited image that
still only uses its palette's colors keeps that palette; otherwise it is
quantised to 256 colors again, with three mips as DEdit made them. See
`DTX.md`, "PS2 textures".

## LTB models (NOLF2, Contract J.A.C.K., Tron 2.0)

**File > Import / Export > LithTech Model (.ltb)**, or drop an `.ltb` on
the viewport. The layout in Blender is the `.abc` one (armature, a mesh per
piece, hidden LOD objects, socket empties, actions); `docs/LTB.md` has the
format. What `.ltb` adds:

| LTB | Blender |
|---|---|
| a LOD's D3D vertices | the mesh's vertices, in their stored order (UV seams are split vertices, as the game draws them) |
| bone sets (up to four nodes for a run of vertices) | vertex groups; the sets themselves in `lt_bone_sets` |
| rigid and vertex-animated meshes | weighted wholly to their node (`lt_bone_effector`) |
| vertex animation frames | shape keys `<animation>\|<key>` |
| a LOD's skins, render style, priority | `lt_textures`, `lt_texture_count`, `lt_render_style`, `lt_render_priority` |
| animation compression | `lt_compression` (0 none, 1 relevant keys, 2 16-bit, 3 16-bit rotations) |

**Skins.** In an extracted game tree the importer reads
`ATTRIBUTES/MODELBUTES.TXT`, finds the model by its `ModelFile` and uses its
`Skin0..N` (NOLF2's `player_action.ltb` gets `CateAction`, `CateCasualHead`
and `CateCasualHair`); elsewhere it guesses as for `.abc`.

**Export.** A mesh imported from an `.ltb` whose vertex and face counts
haven't changed keeps its D3D layout (vertex order, bone sets, the blend
weights where unchanged); any other mesh is laid out afresh: one D3D vertex
per distinct vertex and UV, rigid if one bone (or none) deforms it, else
skeletal with its triangles packed into bone sets of up to four bones (the
strongest four per triangle). Animations keep their compression and the
number of keys each channel had; new ones are written with 16-bit
rotations (the most common kind) and a key per keyframe where a channel
changes. The model file version is the imported one, else 23, which every
shipped engine loads (*Version* chooses).

Data Blender can't hold is kept for the export as well: keys that share a
time (an F-curve can't key a frame twice), non-unit quaternions (some
sockets and animations), bind matrices with scale, UVs of vertices no face
uses, the stored normals and blend weights.

**Round trip.** `tests/blender_roundtrip_ltb.py` imports, exports and
compares every distinct shipped model: 2,273 (NOLF2, Contract J.A.C.K.,
Tron 2.0 and the tools' and Jupiter builds' samples), all within float
precision: positions within 0.001 units, rotations within 2e-7, and
everything else exact, the allocation counts the engine sizes its memory
by included. The 31 chunked files from before the format settled (the
games can't load them) are read and written back but not imported.

## PS2 models (NOLF on the PlayStation 2)

**File > Import > LithTech Model (.ltb)** reads them too (it tells them by
their file type); **File > Export > LithTech PS2 Model (.ltb)** writes one.
`docs/PS2LTB.md` has the format. The layout in Blender is the `.abc` one.
What the PS2 adds:

| PS2 | Blender |
|---|---|
| triangle strips | faces; rigid vertices told apart by position and normal (as the PC models' are) and moved from their node's space to model space; skeletal meshes from their unique vertices |
| strip vertices' s t | the UV map |
| names kept only as hashes | names from `lithtech/ps2ltb_names.txt`; an unknown one is `#xxxxxxxx` and exports as that hash |
| a face on the same vertices as another (double-sided meshes) | its own copies of them (Blender's edit tools need it); `lt_ps2_unique` gives each vertex's number in the file |
| the strips, raw positions and normals, stored weights and keys | `lt_ps2_strips`, `lt_ps2_position`, `lt_ps2_normal`, `lt_ps2_weights`, `lt_ps2_bones`, `lt_ps2_bone_map`, an action's `lt_ps2_tracks` |

**Export.** Each value the import kept is written back while what Blender
holds still matches it (positions, normals, weights, keys and socket
transforms to within what Blender stores them as), so an unchanged model
exports byte for byte: `tests/blender_roundtrip_ps2ltb.py` checks all 1,335
models on both discs. An edited or new mesh is rebuilt: strips by a greedy
walk, packed into VU buffers as the shipped files are, rigid when one bone
deforms it all, else skeletal with the four strongest weights in 1/4096
(`tests/blender_rebuild_ps2ltb.py` checks every model's meshes rebuild with
the same triangles and weights). Nothing exported here has been run on a
PS2. Skins are found as for `.abc` (SKINS beside MODELS, SKINS_PV beside
MODELS_PV, in any letter case); PS2 skins are 8-bit palette DTX files.

## LithTech 1 models (Shogo, Blood 2, KISS Psycho Circus)

**File > Import / Export > LithTech Model (.abc)** reads and writes `.abc`
version 6 too; *Version* "As Imported" writes a v6 model back as v6, and
"12" converts it. `lithtech/abc6.py` has the format. The layout in Blender
is the `.abc` one. What v6 adds:

| v6 | Blender |
|---|---|
| one mesh, each vertex on one node | one mesh object, each vertex in one vertex group |
| no bind pose: the first key of the first animation | the rest pose |
| vertex animation: a deformation node's vertices keyed per keyframe as three bytes each | shape keys `<animation>\|<key>` (as for `.ltb`), at the bind pose, keyed in the action's KEY slot so they play with the bones |
| quaternions as stored, some not unit length | the action's `lt_v6_keys` |
| bounds, the reduced-detail vertices, the vertex bytes, the raw vertex and face values | `lt_v6_*` properties and attributes |
| chunks v6 doesn't define (KISS's `TransformInfo`, in every one of its models) | `lt_v6_extra_chunks_json`, written back as stored |

**Export.** Each value the import kept is written back while what Blender
holds still matches it, so an unchanged model exports byte for byte:
`tests/blender_abc6.py` checks every v6 model of Shogo's and Blood 2's
demos and the LithTech 2.42 SDK's samples, 414. Outside Blender,
`tests/roundtrip_abc6.py` reads and writes back the shipped games' v6
models: all 1,645 of Blood 2's (with The Nightmare Levels) and KISS's come
back byte-identical. An edit changes what it
touches: `tests/blender_abc6_edit.py` moves an animated vertex in one frame
(three bytes change), a bone at one key (that key changes, written unit
length), a vertex, a face, and makes vertex animation on a model that had
none, and checks each file against what Blender shows. v6 holds one node per
vertex (the heaviest of several is taken), triangles only, and no skins; a
node with a vertex that moves in a frame lists every vertex on it, as every
such node in the shipped files does. The first key of the first animation
is LithTech 1's bind, so keep it the rest pose (export warns when it is
not). The shape keys are found by their action's name, so rename an action
and its keys together. Blender interpolates normalized quaternions where LithTech 1
interpolates the stored ones, so a few Blood 2 animations with very long
keys play differently between keys in Blender; the file is unaffected.

## Not yet

- Matrix-palette skeletal meshes are read and written but no shipped model
  uses them, so they're untested.
- World files (`.dat`; the PS2's world `.ltb` files).
- The skins NOLF's attribute files give weapon views' hands (the PC's and
  the PS2's).
- Reading the attribute files of NOLF and AvP2 to pick `.abc` skins (`.ltb` models already use MODELBUTES.TXT).

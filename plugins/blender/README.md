# LithTech for Blender

Import and export of LithTech models and textures in Blender (5.2 LTS and
later):

| Game | Models | Textures |
|---|---|---|
| No One Lives Forever, Aliens versus Predator 2 (and Primal Hunt) | `.abc`, versions 9 to 12 | DTX version -5 |
| No One Lives Forever 2, Contract J.A.C.K., Tron 2.0 | `.ltb`, model file versions 18 to 25 | DTX version -5 |
| No One Lives Forever (PS2) | `.ltb`, file type 2, version 16 | DTX version -5, 32P (palette alpha as the PS2 draws it) |
| Die Hard: Nakatomi Plaza | wrapped `.ltb`, ABC or D3D body, import and conversion | DTX version -5 |
| Shogo, Blood 2, KISS Psycho Circus (LithTech 1) | `.abc`, version 6, vertex animation as shape keys (`lithtech/abc6.py`, `export_abc6.py`; `tests/blender_abc6.py`, `tests/blender_abc6_edit.py`) | (DTX -2 and -3: `lithtech/dtx.py` as for the others) |

meant to grow to every LithTech-era game.

- `io_scene_lithtech/`: the Blender extension (`blender_manifest.toml`),
  mapping models onto armatures, meshes, vertex groups and actions, and
  textures onto images. See `docs/BLENDER.md` for installing and using it.
- `io_scene_lithtech/lithtech/` (also reachable as `lithtech/`): the file
  formats as a plain Python library, no Blender: read a file into objects,
  write them back byte for byte. It lives inside the extension so it ships
  with it.
- `docs/`: the formats as found, with the evidence for each field (the
  shipped tools in `../../talon`, the engine reconstructions, Build 69).
- `tests/`: `roundtrip_abc.py`, `roundtrip_ltb.py`, `roundtrip_ps2ltb.py` and
  `roundtrip_dtx.py` read and write back every shipped model and texture
  (`extract_abc.py` and `extract_ltb.py` fill the asset cache first; the PS2
  files come from the discs' `LITH_PS2.REZ`, out/assets/ps2), and
  `roundtrip_ps2_dtx.py` every PS2 texture. `roundtrip_abc6.py` does the
  same for the LithTech 1 `.abc` (v6) under the folders it's given. `blender_roundtrip.py`,
  `blender_roundtrip_ltb.py`, `blender_roundtrip_ps2ltb.py` and
  `blender_roundtrip_ps2_dtx.py` (run inside Blender) import and export them
  through the extension, and `blender_rebuild_ps2ltb.py` checks the PS2
  exporter's rebuilding of edited meshes. `blender_abc6.py` and
  `blender_abc6_edit.py` do the same for LithTech 1's `.abc` v6. `build_ps2ltb_names.py` makes the
  PS2 models' name list.

Game data is never committed; tests read it from the installed games
(`~/src/rez/games`) through `~/src/tools/rezextract.py`.

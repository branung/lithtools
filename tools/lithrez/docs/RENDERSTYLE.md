# Render styles (.ltb file type 5) and compiled shaders

NOLF2, Contract J.A.C.K. and Tron 2.0 describe how a model piece is drawn
with a render style: a lighting material and up to four render passes,
each with its blend, z-buffer, cull, alpha test and fill states and four
texture stages. They ship as `.ltb` files (mostly in `RS` folders) whose
LTB header says file type 5 instead of a model's 1.
`lithtech::renderstyle` (`core/include/lithtech/renderstyle.hpp`) reads
them; `lithtech::ltb::read` refuses them with that explanation, and
LithRez Studio's `.ltb` preview picks the model or render style view by
the type byte. The compiled shaders some styles and effects use (`.ps`,
`.ash`) are read by `lithtech::shader`.

## Evidence

- The engine's reader, `CD3DRenderStyle::Load_LTBData`: in the games'
  own `Lithtech.exe` (NOLF2 EU at 0x515ce0; Tron 2.0 1.030's and Contract
  J.A.C.K.'s read the same sizes: 0x14, 0x44, 0x214, 1, 1, 0xa0), and in
  source in nolf2src's `src/Engine/runtime/render_a/src/sys/d3d/d3d_renderstyle.cpp`
  (Monolith's NOLF2 release) and Build 69's (a later version, below).
- The structures, `sdk/inc/ltrenderstyle.h`: NOLF2's (nolf2src) and Tron
  2.0's (tron20src) are the same, laid out by MSVC (enums and `uint32` 4
  bytes, `bool` 1, aligned to 4); the LTB header is `LTB_Header` in
  `runtime/model/src/ltb.h`.
- The writer, Build 69's `tools/RenderStyle_Packer/renderstylepacker.cpp`
  (`PackedRenderStyle`), and RenderStylesEditor for the assembled vertex
  shaders (`VertexShaderDlg.h`, `OnVertexShaderSaveAssembled`).

## Layout

Little-endian:

| Size | Field | Meaning |
|---|---|---|
| 20 | `LTB_Header` | `uint8` file type (5, `LTB_D3D_RENDERSTYLE_FILE`; 6 is the PS2's), pad, `uint16` version (3, `RENDERSTYLE_D3D_VERSION`), `uint8` and 3 `uint32` reserved (with padding). The packer sets only the type and version, so the rest is whatever was on its stack |
| 4 | total size | the styles' sizes added up; refused over 1,000,000 |
| 4 | style count | refused over 32. The engine takes the first style that compiles and that the device supports |
| | styles | |

A style:

| Size | Field | Meaning |
|---|---|---|
| 4 | size | the bytes that follow, to the end of its last pass |
| 68 | `LightingMaterial` | ambient, diffuse, emissive, specular (4 floats each: r, g, b, a), specular power |
| 4 | pass count | asserted at most 4 |
| | passes | per pass: `RenderPassOp` (532), then a `uint8` saying whether Direct3D options follow |

`RenderPassOp` is four `TextureStageOps` of 120 bytes (11 enums:
texture, colour op and its two arguments, alpha op and its two arguments,
UV source, U and V address, filter; `bool` UV transform at 44, its 16
floats at 48, `bool` projected at 112, `uint32` coordinate count at 116),
then at 480: blend mode, z-buffer mode, cull mode, texture factor (ARGB),
alpha ref, `bool` dynamic light (500), z test, alpha test, fill mode,
`bool` bump env map (516), its stage (520), scale and offset (floats).
The enumerators are `ltrenderstyle.h`'s (`RENDERSTYLE_COLOROP_MODULATE`
and so on); `lithtech::renderstyle::colour_op_name` and its siblings name
them.

The Direct3D options are `RSD3DRenderPass`, a vertex shader's settings,
read as `bool bUseVertexShader` (1 byte), `bool bExpandForSkinning` (1
byte), then 160 bytes into the structure from `ConstVector_ConstReg1`
(its offset 4) on: three constant registers each with a vector, the
registers for the world-view, projection, world-view-projection and
view-projection matrices, the camera position, the lights, the material,
the ambient light and the previous world-view, the last register, and
the vertex declaration (four `bool[4]` and an `int32[4]` UV count). So
the file holds the structure without the 2 bytes of padding after its
two bools: 162 bytes. With a vertex shader, four blocks follow, each a
`uint32` size and that many bytes; the engine prints "Error: Found
renderstyle using vertex shader. These are no longer supported." and
skips them. The shipped blocks are Direct3D 8 vertex shader code
(vs.1.1, from D3DX8's assembler), the four variants RenderStylesEditor
assembles (`VERTSHADER_TYPES_COUNT`: rigid, then skinned).

nolf2src's source reads the options differently from the binaries:
`Read(&D3DRenderPass.bExpandForSkinning, sizeof(D3DRenderPass) -
offsetof(RSD3DRenderPass, bExpandForSkinning))`, 163 bytes from offset 1,
where every game's `Lithtech.exe` reads 1 byte to offset 1 and then 160
to offset 4. The binaries' reading is the one that fits the files.

Build 69's engine is later: its `RSD3DRenderPass` is four fields (vertex
and pixel shader flags and IDs), read one by one, and it reads an
`RSD3DOptions` (an effect shader flag and ID, 8 bytes) after each style.
No file the games ship is laid out that way (a one-pass style is 637
bytes, which leaves no room for the options).

## In the shipped games

`tests/renderstyle_verify <rez>...` reads every `.ltb` of file type 5
(and the `.ps` and `.ash` shaders):

| Game | Render styles | Read | Version 2 | Vertex shader passes |
|---|---|---|---|---|
| NOLF2 (EU) | 46 | 35 | 11 | 2 (`RS/CELL.LTB`) |
| Contract J.A.C.K. (EU) | 51 | 40 | 11 | 2 (`RS/CELL.LTB`) |
| Tron 2.0 1.030 (US) | 172 | 162 | 10 | 1 (`RS/WIREFRAMEA.LTB`) |

Every version 3 file has one style, of one or two passes (Tron 2.0: 131
and 31), and no bytes after it. The version 2 files are the ten
`RENDERSTYLES/RS_*.LTB` of each game's `Engine.REZ` (dated April 2001,
from an earlier packer), and NOLF2's and Contract J.A.C.K.'s
`RS/HOTCOALS.LTB`. `Load_LTBData` refuses anything but version 3, so the
games can't load them either; their layout isn't in any source we have,
and LithRez Studio says so rather than guess.

Three Tron 2.0 styles (`RS/FULLBRIGHTON_ALPHA_SPECULAR.LTB` in
`Game2.rez`, `PROPS/RS/ARCHIVECUBE.LTB` and `CHARS/RS/INNER_GLOW.LTB` in
`Game.rez`) have colour op 19 in their second stage, past the 16 colour
ops of the games' `ltrenderstyle.h`. The renderer turns ops into
Direct3D's through `CRenderStyleLookupTables::m_nColorOp[16]`, so it reads
past the table into the next one, `m_nAlphaOp`: index 19 is
`m_nAlphaOp[3]`, `D3DTOP_MODULATE` (by NOLF2's tables; Tron 2.0's
renderer isn't decompiled). LithRez Studio shows the number.

## Compiled shaders

`.ps` pixel shaders (`PS/*.PS`: NOLF2 and Contract J.A.C.K. 3, Tron 2.0
2), as `CPixelShaderMgr::LoadPixelShader` reads them (nolf2src's
`pixelshadermgr.cpp`): `uint32` `'LTPS'` (`PIXELSHADER_FILE_ID`, the
bytes "LTPS"), `uint32` version (1), `uint32` size, then that many bytes
of Direct3D 8 pixel shader tokens (ps.1.0 and ps.1.1 in the shipped
files).

`.ash` assembled vertex shaders (Tron 2.0's
`WEAPONS/BALL/RS/VS_HALO.ASH`), as RenderStylesEditor's "save assembled"
writes them: `uint32` version (1), `bool` expand for skinning, then one
shader (four when expanded for skinning), each a `uint32` size and its
tokens (vs.1.1).

`.vsh` files are the vertex shaders' assembly source, text.

`lithtech::shader::describe` names a token stream's version and size and
the assembler's comment ("D3DX8 Shader Assembler Version 0.91" in all of
them). `shader_test` (in ctest) checks both formats on files made in the
test, and `renderstyle_test` render styles with every part described
above.

## LithRez Studio

A render style previews as a property tree: the header, then each
style's lighting material (with colour swatches) and passes, each pass's
states, its texture stages (disabled ones folded to one line) and its
Direct3D options with the shader blocks. A version 2 file says it can't
be read and why. A `.ps` or `.ash` shows what it is and its bytes; a
`.vsh` its text.

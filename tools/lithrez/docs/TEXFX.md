# Texture effects (.tfs, .tfg)

NOLF2, Contract J.A.C.K. and Tron 2.0 animate some world textures with
texture effects: a compiled texture script (`.tfs`) computes a texture
transform every frame on a small register machine, and a texture effect
group (`.tfg`) gives a texture's stages their scripts and parameter
values. A world texture names its group, which the engine loads from
`TextureEffectGroups\<name>`. `lithtech::texscript`
(`core/include/lithtech/texscript.hpp`) reads both, disassembles and
decompiles the scripts and runs them, and LithRez Studio previews them.

## Evidence

All in Build 69 (`legacy/jupiter/src/Engine`):

- The compiler, `tools/TSCompiler` (`TextureScriptCompiler.cpp`:
  `ParseScript`, `ParseUserParamList`, `SaveConstants`, `SaveTree`,
  `CalcStackVars`, `IsVar`, `IsFunction`, the token table in `NextToken`).
- The engine's reader and interpreter,
  `runtime/render_a/src/sys/d3d/texturescriptinterpreter.cpp`
  (`LoadScript`, `Evaluate`, the op and function tables), with
  `texturescriptdefs.h` (the variables, ops, functions, inputs, outputs).
- The group reader, `texturescriptmgr.cpp` (`CTextureScriptMgr::
  GetInstance`, `LoadEvaluator`), and how a stage is used,
  `texturescriptinstance.cpp` (`Evaluate`, `Install`).
- The group writer, DEdit's `tools/DEdit/Dlgs/EditEffectGroupDlg.cpp`
  (`SaveEffectGroup`, `LoadEffectGroup`, `LoadScript`).

NOLF2's and Contract J.A.C.K.'s engines (nolf2src, cjacksrc) have the same
interpreter.

## Texture scripts (.tfs)

The source language (TSCompiler):

    UserParams XSpeed, YSpeed;      # up to 6 parameters, set per stage by the group
    Input UV;                       # CSNormal CSPos CSReflection WSNormal WSPos WSReflection UV
    Output TexCoord2;               # TexCoord2 TexCoord3 TexCoord3Proj TexCoord4Proj
    BeginScript
        Mat02 = (Time * XSpeed) % 1.0;
        Mat12 = (Time * YSpeed) % 1.0;
    EndScript

Statements assign expressions to the matrix (`Mat<row><column>`), a
parameter, or nothing else; expressions use numbers, `Time`, `Elapsed`,
`LevelOffsetX/Y/Z`, the parameters, the matrix, `+ - * /`, `%` (fmod),
`<` (min) and `>` (max), unary minus and the functions `Sin`, `Cos`,
`Tan`, `SqrSin`, `SqrCos` (square waves) and `DegToRad`. `#` starts a
comment.

### Layout

Little-endian:

| Size | Field | Meaning |
|---|---|---|
| 2 | version | `TS_CURR_VERSION`, 1; the engine refuses others |
| 4 | parameter block size | 1 + the names with their NULs |
| | parameter block | a `uint8` count, then each name with a NUL. The engine skips it; DEdit reads it to label the values |
| 4 | input | `TSINPUT_*`: 0 CSNormal ... 6 UV |
| 4 | output | `TSOUTPUT_*`: 0 TexCoord2 ... 3 TexCoord4Proj |
| 4 | dirty flags | 1: uses `Time` or `Elapsed`, re-run every frame; 2: uses a parameter, re-run when they change (`GetDirtyFlags`) |
| 4 | constant count | |
| 4 each | constants | floats, each number in the source once (`GetConstant`; at most 32) |
| 4 | code size | bytes |
| | code | instructions of 4 bytes: op, operand 1, operand 2, result |

### The machine

`m_fVarList`, 75 floats, which the operands and results index:

| Index | Variables |
|---|---|
| 0-15 | the stack: the compiler's temporaries (`CalcStackVars`: a node's depth in its statement) |
| 16-31 | the matrix, `Mat<row><column>` = 16 + 4 * row + column; the identity at the start of each run |
| 32-37 | the parameters (`UserParams`, in order) |
| 38, 39 | `Time` (seconds since the effect started) and `Elapsed` (since its last run) |
| 40-71 | the constants |
| 72-74 | `LevelOffsetX/Y/Z`, the world's source offset |

Ops: 0 ADD, 1 SUBTRACT, 2 MULTIPLY, 3 DIVIDE, 4 BIND (`fmod`),
5 FUNCTION (operand 1 is the function, operand 2 its argument), 6 NEGATE
(operand 1), 7 ASSIGN (operand 1), 8 MIN, 9 MAX. Each instruction stores
`op(var[p1], var[p2])` into `var[out]`; the compiler turns `X = e` into
`e`'s code with its last result going to `X` (an ASSIGN when `e` is a
plain variable). After the run the matrix is the transform, and the
parameters are written back (a script can keep state in them).

The engine checks only the version: an op past 9, a function past 5 or an
operand past 74 would index past its tables.
`lithtech::texscript::check` reports them; no shipped script has any.

**The engine's Sin is the cosine and its Cos the sine:**
`texturescriptinterpreter.cpp` has `TSFuncSin` returning `cos(fVal)`
and `TSFuncCos` returning `sin(fVal)`, in the order of `TSFUNC_SIN` and
`TSFUNC_COS`, so a script's `Sin(x)` runs as `cos(x)` (a retail bug, in
NOLF2's, Contract J.A.C.K.'s and Tron 2.0's engines; Contract
J.A.C.K.'s map file lists both functions in `texturescriptinterpreter.obj`).
The shipped scripts were tuned on it: a rotation built from them is still
a rotation, a quarter turn off. `lithtech::texscript::evaluate` and
LithRez Studio run them as the engine does; the disassembly notes it.

### The matrix

`CTextureScriptInstance::Install` hands the matrix to Direct3D transposed
(Direct3D's row vectors), so the coordinates are transformed as a column
vector, `M * (x, y, z, w)`. For `UV` input Direct3D passes the two
texture coordinates as `(u, v, 1, 0)`, so a UV script's translation is in
`Mat02` and `Mat12` (as `UVPan.tfs` puts it); for positions it is
`Mat03`/`Mat13`. A world-space input is first turned from camera space by
the inverse view (the full matrix for positions, the rotation for normals
and reflections). The output type sets `D3DTTFF_COUNT2`, `COUNT3`, or
`COUNT3`/`COUNT4` with `D3DTTFF_PROJECTED`.

## Texture effect groups (.tfg)

| Size | Field | Meaning |
|---|---|---|
| 4 | version | 1; the engine refuses others |
| 4 | stage count | DEdit writes 2; the engine reads at most 2 (`NUM_STAGES`) |
| | stages | per stage: a `uint32` type, then by type |

Stage types: 0 disabled (nothing follows), 1 overridden (`uint32`
channel, then `uint32` stage: use that stage's transform), 2 evaluated
(`uint32` channel; the script's path as `ILTStream::ReadString` reads a
string: a `uint16` length and the characters; a `uint32` value count and
the values, floats). Channels (`ETextureScriptChannel`, DEdit's names):
0 none, 1 Base Texture, 2 Detail Texture, 3 Environment map, 4 Light Map,
5 Dual Texture. DEdit writes six values (one per parameter slot, 0 past
the script's parameters); the engine keeps up to six.

The script path is from the root of the game's resources, with a leading
backslash (`\Tex\TexFX\UVPan.tfs`), and may be in another of the game's
archives: Tron 2.0 keeps its groups in `Game.rez` and their scripts in
`Game2.rez`.

## In the shipped games

`tests/texfx_verify <rez>...` reads every script and group of a game's
archives, checks the scripts, and looks each group's scripts up:

| Game | Scripts | Groups | Failed | Problems | Scripts not found |
|---|---|---|---|---|---|
| NOLF2 (EU) | 5 | 28 | 0 | 0 | 0 |
| Contract J.A.C.K. (EU) | 5 | 28 | 0 | 0 | 0 |
| Tron 2.0 1.030 (US) | 15 | 273 | 0 | 0 | 0 |

Every script is version 1 with no bytes after its code. Every group has
2 stages and no bytes after them; no stage is overridden. Tron 2.0's 546
stages: 281 evaluated (261 on the base texture, 19 on the environment
map, 1 on the detail texture), 265 disabled; NOLF2's and Contract
J.A.C.K.'s 56 each: 30 evaluated (22 base, 7 dual texture, 1 environment
map), 26 disabled. Every evaluated stage has a value for each of its
script's parameters. Inputs: UV (Tron 2.0 6,
NOLF2 2), WSPos (4, 2), CSPos (Tron 2.0 3), WSReflection (2, 1); every
output is TexCoord2. `texfx_verify --dump` prints them all decompiled,
for example NOLF2's `TEXTURESCRIPTS/WSPOSXZPAN.TFS`:

    UserParams XSpeed, ZSpeed, XScale, ZScale, XOffset, ZOffset;
    Input WSPos;
    Output TexCoord2;
    BeginScript
        Mat00 = XScale;
        Mat11 = 0.0;
        Mat12 = ZScale;
        Mat03 = XOffset + ((Time * XSpeed) % 1.0);
        Mat13 = ZOffset + ((Time * ZSpeed) % 1.0);
    EndScript

`texscript_test` (in ctest) builds Tron 2.0's `UVPan.tfs` as TSCompiler
builds it from the source above (the same 69 bytes as the shipped file)
and checks the reading, the disassembly, the decompiled source and the
evaluation.

## LithRez Studio

A `.tfs` shows its parameters, input, output, when it re-runs, constants,
the decompiled source and the disassembly, and runs: the matrix as the
script leaves it each frame, the parameters editable (and updated, as the
engine writes them back), and for a UV script a test pattern moved by the
matrix. A `.tfg` shows each stage's type, channel, script (a link to it,
when the archive has it) and values by the script's parameter names, and
runs its evaluated stages with those values.

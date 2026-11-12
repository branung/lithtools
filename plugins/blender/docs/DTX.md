# DTX textures

LithTech's texture file, version -5. Every `.dtx` we have, from NOLF (2000)
to Tron 2.0 (2003), is version -5 and uses this one layout: 21,955 distinct
files across NOLF (all languages and GOTY), AvP2 Primal Hunt, NOLF2,
Contract J.A.C.K., Tron 2.0, the Tron 2.0 tools and the Jupiter Build 51/69
game trees. All of them read and write back byte for byte with
`lithtech/dtx.py` (three 0-byte Tron files are not textures; see
[Odd files](#odd-files)).

The older engines wrote 8-bit palettized textures, versions -2 (LithTech
1.0, Blood 2, 1998) and -3 (LithTech 1.5, KISS Psycho Circus, 2000); see
[Versions -2 and -3](#versions--2-and--3). Both `dtx.py` and the C++
library (`tools/lithrez`, `lithtech::dtx`) read and decode them, and
`dtx.py` writes them back byte for byte: every one in the Blood 2 (EU, the
Nightmare Levels, the demo) and KISS Psycho Circus (DE, EU, 1.11) installs,
4,315 and 1,513 distinct files, the two decoding alike (FNV of every top
level). Five `.dtx` names in Blood 2 hold something else (see
[Odd files](#odd-files)).

Evidence, in order of weight:

- **talon**: `legacy/talon/src/LT2/tools/shared/engine/dtxmgr.h`, `dtxmgr.cpp`
  and `pixelformat.h`/`.cpp`. This is the engine code the 2001 tools
  (DEdit, Processor) were built with. `dtx_Create` reads the format and
  `dtx_Save` writes it.
- **Build 69**: `legacy/jupiter/src/Engine/runtime/shared/src/dtxmgr.*` and
  `pixelformat.cpp`, the same format two years on. Also DEdit's
  `legacy/jupiter/src/Engine/tools/DEdit/TextureDlg.cpp` and
  `Misc/TextureHelper.cpp`, which write the palette and cube-map sections.
- **The files themselves**: every claim below was checked against all
  21,955 files (`tests/roundtrip_dtx.py`).

All values are little-endian.

## Layout

```
DtxHeader                   164 bytes
pixels of mip 0..n-1        CalcImageSize(bpp, w>>i, h>>i) bytes each
section 0..m-1              32-byte SectionHeader + m_DataLen bytes
```

Nothing follows the last section in any file.

The comment in `dtxmgr.h` describes something different: alpha masks after
the mips, and a "1 if more sections, 0 if no more" word before each
section. That comment is out of date. `dtx_Create` reads exactly
`m_nSections` sections, back to back, with no marker, and `dtx_Save` writes
them that way. The files agree.

## DtxHeader (164 bytes)

| Offset | Type | Field | Notes |
|---|---|---|---|
| 0 | uint32 | `m_ResType` | `LT_RESTYPE_DTX` = 0; `dtx_Create` rejects anything else |
| 4 | int32 | `m_Version` | -5 (`CURRENT_DTX_VERSION`); `dtx_Create` rejects anything else |
| 8 | uint16 | `m_BaseWidth` | |
| 10 | uint16 | `m_BaseHeight` | |
| 12 | uint16 | `m_nMipmaps` | 1..15 accepted by talon's `dtx_Create`; Build 69 caps it at `MAX_DTX_MIPMAPS` = 8. Shipped files use 1 to 8 (mostly 4) |
| 14 | uint16 | `m_nSections` | valid only with `DTX_SECTIONSFIXED` |
| 16 | int32 | `m_IFlags` | see [Flags](#m_iflags) |
| 20 | int32 | `m_UserFlags` | defined by the game; see below |
| 24 | uint8[12] | `m_Extra` | see [m_Extra](#m_extra) |
| 36 | char[128] | `m_CommandString` | NUL-terminated, then garbage (see below) |

`m_UserFlags`: Processor copies it into each world texture's info as a
`uint16` (`GetWorldTextureInfo`, talon `PreProcessor/gettextureinfo.cpp`).
Shipped values are small numbers (0x14, 0x1e, 0x0a, 0x07…), which look like
the games' surface type ids. That reading is not confirmed. The library
keeps the field as a plain int.

**Command string.** Shipped command strings hold render hints such as
`Detailtex Tex\DetailTex\Dtl0001.dtx`, `EnvMap Tex\EnvMap\EnvMap011.dtx`,
`alpharef 96` and `mipmapbias -1`. 388 different strings occur.

In about half the files, the bytes after the NUL are not zero. They are
leftover memory from the tool that saved the file: `dtx_Save` memcpys the
whole in-memory header. The library keeps all 128 bytes as
`command_raw`, and `command` gives you the text.

### m_IFlags

From `dtxmgr.h`. `dtx_Save` masks the field with `DTX_FLAGSAVEMASK`.

| Bit | Name | |
|---|---|---|
| 0 | `DTX_FULLBRITE` | |
| 1 | `DTX_PREFER16BIT` | |
| 2 | `DTX_MIPSALLOCED` | runtime only; not saved |
| 3 | `DTX_SECTIONSFIXED` | set in every file. Without it, `dtx_Create` reads no sections and treats the count as 0 |
| 6 | `DTX_NOSYSCACHE` | |
| 7 | `DTX_PREFER4444` | |
| 8 | `DTX_PREFER5551` | |
| 9 | `DTX_32BITSYSCOPY` | |
| 10 | `DTX_CUBEMAP` | faces 2-6 are in a `CUBEMAPDATA` section |
| 11 | `DTX_BUMPMAP` | |
| 12 | `DTX_LUMBUMPMAP` | |

Shipped combinations: 0x8, 0x9, 0x108, 0x88, 0x10a, 0x109, 0xa, 0x89, 0x408,
0x8a, 0x808, 0x1008, 0xb and 0x288.

### m_Extra

The meanings come from the `DtxHeader` comment and its accessors in
`dtxmgr.h`.

| Byte | Meaning | Shipped values |
|---|---|---|
| 0 | texture group (Build 69 `dtx_Create` adds `g_CV_TextureGroupOffset[group]` to the mip offset) | 0-10, one 16, one 100 |
| 1 | mipmaps to use (set by DEdit's texture properties) | 0 in 11,072 files (NOLF-era tools left it unset); otherwise usually `m_nMipmaps` |
| 2 | BPPIdent; 0 means `BPP_32` (`GetBPPIdent`) | 0, 3-7 |
| 3 | non-S3TC mipmap offset | 0, 1, rarely 2-7 |
| 4 | UI mipmap offset; scale = `1 << n` | 0-2 |
| 5 | texture priority | always 0 |
| 6-9 | detail texture scale, a float stored as value - 1.0 (so zeroed bytes mean 1.0) | 1.0, 6.0, 4.0, 5.0, … |
| 10-11 | detail texture angle, int16 | 0, 90, rarely 1 or 45 |

## Pixel formats

BPPIdent values come from `pixelformat.h`. Level sizes come from
`CalcImageSize` in `pixelformat.cpp`, which talon and Build 69 share.

| Id | Name | Bytes per level | Shipped |
|---|---|---|---|
| 0 | `BPP_8P` | w·h | never: a stored 0 reads as `BPP_32` |
| 1 | `BPP_8` | w·h | none |
| 2 | `BPP_16` | w·h·2 | none |
| 3 | `BPP_32` | w·h·4 | yes |
| 4 | `BPP_S3TC_DXT1` | w·h / 2 | yes |
| 5 | `BPP_S3TC_DXT3` | w·h | yes |
| 6 | `BPP_S3TC_DXT5` | w·h | yes |
| 7 | `BPP_32P` | w·h | 6 PC files (NOLF GOTY, NOLF2); 8,746 on the PS2 discs |

- **Mip sizes.** Each mip halves both dimensions with a shift (`dtx_Alloc`).
  S3TC sizes are **not** rounded up to whole 4×4 blocks: by `CalcImageSize`,
  a 2×2 DXT1 level would be 2 bytes. No correctly formed shipped file has a
  compressed level smaller than 4 pixels on a side, so this never comes up.
  The library uses `CalcImageSize` exactly. Its decoder pads a short level
  with zeros. Its encoder truncates to the same size.
- **Row order.** Rows are stored top row first. Decoded skins come out
  upright.
- **BPP_32.** Each pixel is a PValue: a `uint32` 0xAARRGGBB
  (`ltpvalue.h` masks), so the bytes are B, G, R, A.
- **BPP_32P.** One byte per pixel, an index into the `PALLETE32` section
  (spelled that way) of 256 four-byte entries. Talon `CC_32PtoBF` builds
  the PValue as `b<<24 | g<<16 | r<<8 | a` from an
  `RPaletteColor {a, r, g, b}` whose field names are off by one. In memory
  the entries are B, G, R, A, the same as PValue. DEdit's `Write8BitDTX`
  writes the section from a 256 × 4-byte palette.
- **S3TC** blocks follow the standard DXT layout:
  - DXT1: 8 bytes per block, two RGB565 colors and 32 bits of 2-bit indices.
  - DXT3: 8 bytes of 4-bit alpha, then a DXT1 color block.
  - DXT5: two alpha endpoints, 48 bits of 3-bit indices, then a color block.

  The decoder follows the engine's software path (`ConvertDXTGeneric`)
  with one exception:
  - **565 expansion:** `j*255/max`, truncated (`FormatMgr::InitScaleTables`).
  - **Interpolation:** truncating divisions.
  - **DXT3 alpha:** `n*17` (`g_FullAlphaValues`).
  - **DXT1:** 4 colors when c0 > c1. Otherwise 3 colors, and the fourth is
    transparent black.
  - **The exception:** DXT3 and DXT5 always use 4 colors, as the Direct3D
    hardware that drew the games did. The engine's software path tests
    c0 > c1 for these too.
- **BPP_16.** No file says what layout is stored: `dtx_SetupDTXFormat2`
  gives `BPP_16` no masks. The decoder reads RGB565, the format manager's
  `m_RGB565Format`, unless the flags prefer 4444 or 5551. This is unverified.

## Sections

```
char   m_Type[15]     NUL-terminated
char   m_Name[10]     NUL-terminated
(3 bytes of struct padding)
uint32 m_DataLen
uint8  data[m_DataLen]
```

`SectionHeader` in `dtxmgr.h` is 25 bytes of chars and a `uint32`. MSVC
aligns it to 32 bytes, and `dtx_Save` writes `sizeof(m_Header)`. The
padding bytes, and the bytes after each NUL, are leftover memory in many
files. The library keeps them all.

`dtx_Create` links sections in reverse order: it pushes each one onto the
head of the list. `dtx_Save` writes them in list order, so a load and save
in the tools reverses them. The library keeps the file's order.
`dtx_FindSection` matches the type exactly, with `strcmp`.

Section types:

- **`PALLETE32`** (6 PC files, every PS2 32P texture): the 256-color
  palette for `BPP_32P`, 1024 bytes. See "PS2 textures" below for its
  alpha.
- **`CUBEMAPDATA`** (76 files, all with `DTX_CUBEMAP`): faces 2-6 of a cube
  map. The base mips in the file are the first face. This section holds the
  other five, each with all its mips at the base face's sizes, concatenated:
  `m_DataLen` = 5 × (sum of mip sizes). The face order comes from the
  `DTX_CUBEMAP` comment, "-x,+y,-y,+z,-z", so the base is +X. DEdit builds
  the section in `TextureHelper.cpp` (`dataSize * 5`).
- **`LIGHTDEFS`**: texture lights. Processor reads them (talon
  `gettextureinfo.cpp`, `ParseLightDefs`) as one command per light:
  `<center|dirlight|omni> <u> <v> [Z:<z>] <radius> [<r g b>] [<r g b>]`.
  No shipped file has this section; it is a source-art feature.

## Mipmaps

DEdit regenerates mips with `dtx_BuildMipmaps` (talon `dtxmgr.cpp`,
called from Build 69 `TextureDlg.cpp` `CreateNewMipTexture`). Each level is
the 2×2 box average of the level above it, per byte, truncated
(`sum >> 2`). `build_mipmaps()` does the same. It reproduces the stored
mips of 5,180 of the 5,562 multi-mip 32-bit files exactly (322 of 330 in
AvP2 Primal Hunt, 1,566 of 1,674 in NOLF). The rest were presumably mipped
some other way.

DEdit's `MaxMipsForSize` allows mips down to 8 pixels. `from_rgba()` uses
that as its default count, capped at 8.

The S3TC compressor the original tools used is not available, so encoding
cannot reproduce a shipped compressed file. Only parsing and writing are
byte-exact. `encode_pixels()` is a simple range-fit compressor.

## Versions

`read()` dispatches on `m_Version` through `_READERS`, and only -5 is
there:

- NOLF, AvP2 and PH, NOLF2, Contract J.A.C.K. and Tron 2.0 all ship -5.
  Talon and Build 69 both define `CURRENT_DTX_VERSION` as -5 and reject
  everything else.
- -2 (Blood 2) and -3 (KISS Psycho Circus) are below. The C++ reader has
  them (`read_old` in `tools/lithrez/core/src/dtx.cpp`); a Python one would
  go in `_READERS` beside `_read_v5`. -4 (Shogo? the Blood 2 add-on?) and
  the Jupiter EX versions are not in our data.

## Versions -2 and -3

8-bit palettized textures with optional 4-bit alpha masks. There is no
source; the layout comes from the engines' own loaders and renderers,
disassembled, and every claim was checked against all the files we have:

- **Blood 2 demo** (`B2Demo.rez`): 519 textures, all -2. The engine is the
  demo's `Client.exe` and `d3d.ren`.
- **KISS Psycho Circus 1.11** (`PSYCHO.REZ`): 2,929 textures, all -3. The
  engine is `KPC_V111/Psycho/client.exe` and `d3d.ren`.

Every file is exactly the size the layout below gives; there are no
sections in any of them, and nothing follows the last mask.

### Layout

```
header                  36 bytes (-2) or 164 bytes (-3)
palette id              8 bytes
palette                 256 x 4 bytes
pixels of mip 0..n-1    w>>i x h>>i bytes each, one block
masks of mip 0..n-1     only with flag bit 1, one block after all the pixels
section 0..m-1          only with DTX_SECTIONSFIXED: as in -5
```

`dtx_Create` (Blood 2 `Client.exe` 0x420d80; KISS `client.exe` 0x1cc8b40)
reads the header (0x24 bytes; KISS 0xa4), rejects any other version (-2;
KISS -3) and a mip count of 0 or over 15, then reads 0x408 bytes (the id
and the palette) and hands them to the palette cache (0x43c820; KISS
0x1cefa00). `dtx_Alloc` (0x420c20; KISS 0x1cc89c0) sizes one allocation for
all the levels and, with flag bit 1, the masks, and gives each level a
20-byte entry {width, height, pitch, pixels, mask}. `dtx_Create` then reads
all the pixels in one call and all the masks in a second one, and the
sections as -5 does (32-byte header, data length at 28).

### Header

| Offset | Type | Field | Notes |
|---|---|---|---|
| 0 | uint32 | res type | 0 |
| 4 | int32 | version | -2 or -3 |
| 8 | uint16 | width | |
| 10 | uint16 | height | |
| 12 | uint16 | mip count | 1..15; KISS always 4, Blood 2 4 (16 files 1) |
| 14 | uint16 | section count | 0 in every file |
| 16 | int32 | flags | see below |
| 20 | int32 | user flags | as -5: small numbers (Blood 2: 0, 10, 20 ... 110; KISS also bit masks up to 0x8000) |
| 24 | uint8[12] | ? | see below |
| 36 | char[128] | command string | -3 only |

Bytes 24-35: in -3, bytes 28-31 are a float and 32-33 a uint16 that KISS's
`d3d.ren` turns into its detail-texture matrix (0x10023f30 builds it from
the texture's +0x20 and +0x24, which are header bytes 28 and 32, since the
header is copied to +4; the detail pass at 0x1000c6f0, gated on the
`DetailTextures` variable, reads the matrix). They are the detail scale
(stored as is, not as -5's value - 1: 6.0 in 1,977 files, 0 in the other
952) and angle (0 in every file). In Blood 2 bytes 28-35 are always zero.
Bytes 24-27 hold small values in both games (Blood 2 `05 04 88 00`,
`00 02 88 80`; KISS `02 01 88 00`, `00 02 00 00`) that neither engine is
seen to read; byte 26 is 0, 0x88 or 0xec where -5 has its BPP ident. The
library keeps bytes 24-35 as `extra`.

The -3 command string is never text in KISS: in 2,790 files it is empty,
and in the rest leftover memory (fragments like `sible`). The library
keeps it only when it is printable.

### Flags

| Bit | Meaning | Evidence |
|---|---|---|
| 0 | FULLBRITE | the renderer copies it into its texture (Blood 2 `d3d.ren` 0x1001e640; KISS 0x10023f30), and Blood 2 picks the surface format by it |
| 1 | alpha masks | `dtx_Create`/`dtx_Alloc` read and size the masks; the upload ORs them in (below). -5 has PREFER16BIT here |
| 3 | SECTIONSFIXED | as -5 (set in most files, but the count is always 0) |
| 5 | never paletted | 0x1001e640: without it a plain, not-fullbright texture may go to an 8-bit paletted surface; with it, to 16-bit. Skies (`SKY/STARZ2`, `SKYPAN9`), user flags 0x6e |

Shipped: Blood 2 0xa (273 files), 0x8 (185), 0x9 (41), 0x0 (17), 0x1 (2),
0x28 (1); KISS 0x8 (1,825), 0xa (968), 0x9 (96), 0x28 (20), 0xb (16), 0x2a
(4). The library calls bit 1 `DTX_OLD_ALPHAMASKS` and bit 5
`DTX_OLD_NOPALETTED` (the engines' names are not known).

### Palette

The 8-byte id is the palette cache's key: 0x43c820 hashes its first dword
(mod 30) and shares a palette between textures whose id and 1,024 bytes
match. It is not pixel data (96 distinct ids in Blood 2, 1,322 in KISS).

Each entry is 4 bytes, **{unused, R, G, B}**. The upload's 16-bit table
(Blood 2 `d3d.ren` 0x1001d718) takes red, green and blue from bytes 1, 2
and 3 (rounded and shifted into the surface format); for an 8-bit
paletted surface the same function builds a DirectDraw palette of
`PALETTEENTRY {peRed, peGreen, peBlue}` = bytes 1, 2, 3 and calls
CreatePalette with 0x44 (DDPCAPS_8BIT | DDPCAPS_ALLOW256). Byte 0 is 0xff
in all of KISS and in all but 16 Blood 2 files (0 there), and nothing reads
it. KISS's table builder (`d3d.ren` 0x100227d0) is the same, with a
brightness factor.

**Entries 246-255 are always opaque.** The table builder then ORs the
surface format's alpha mask (`dwRGBAlphaBitMask`, the format entry's +0x28)
into entries 246 to 255, in every format it builds (Blood 2 0x1001d718;
KISS 0x100227d0, the same loop of 10 from `+ 0xf6`). In the alpha formats a
masked texture uses, those ten colours stay opaque whatever the mask says.
They are the fullbright colours: a FULLBRITE texture is uploaded in a
format with alpha, which marks them. Blood 2's masked textures never put
those indices under a partial mask; 36 of KISS's do (51,153 texels, mostly
Shogo explosion sprites and model skins such as `SKINS/DECOS/DRUMS.DTX`,
whose unused black areas stay opaque). The library draws them as the
engine does.

### Masks

One mask per level, after all the levels' pixels. A level of n = w·h
pixels has n/2 mask bytes, or 1 byte if n is 1 (`dtx_Alloc`: `n > 1 ? n
>> 1 : n`); 4 levels of 8×8 give 32 + 8 + 2 + 1 = 43.

The D3D upload (Blood 2 `d3d.ren` 0x1001db60; KISS 0x100242b0) reads each
row as w/2 bytes and ORs each byte into two 4444 texels: the **low nibble**
is the alpha of the even pixel (`byte << 12`), the **high nibble** that of
the odd one (`(byte & 0xf0) << 8`). Alpha is the nibble × 17. A level one
pixel wide gets no mask bits (w/2 is 0): it is clear, except for entries
246-255. Flag bit 1 also sends the texture to the alpha format (0x1001e640;
the alpha format list at 0x1004c7b0 prefers 4444, then 1555).

The library draws a texture without masks opaque: its tables' alpha bits
mark only the fullbright entries, not coverage. (How the renderer uses that
alpha for FULLBRITE textures was not traced.)

## Odd files

- `blood2-eu BLOOD2.REZ:SKINS/GRYCRET1.dtx`, `METAL1.dtx`, `ORGBRIKB.dtx`,
  `CRATE1.dtx` and `TEXTURES/GREYS/DULL1_TRANS.dtx` (the first four also in
  the Nightmare Levels' `BLOOD2.REZ`) are not DTX files: they start
  `04 00 07 00` or `04 00 08 00` (resource type 0x70004/0x80004) with a
  128×128 size at offset 0x12, an older texture format the game evidently
  doesn't load. Neither reader takes them.

- `tron2-usa Game2.rez:TEX/PROTOTYPE/M09/DAN/PANGLOWEDIT.dtx`,
  `PANGLOWEDIT2.dtx` and `TEX/PROTOTYPE/REALWORLD/EVILLAB/BASE/ELEVATOR04.dtx`
  are 0 bytes (in 1.0 and 1.030 alike), and no world, sprite, effect group
  or attribute file names them. The engine couldn't load them: its rez
  streams zero what they can't read, so `dtx_Create` (Build 69's
  `runtime/shared/src/dtxmgr.cpp`) would see version 0 and return
  `LT_INVALIDVERSION`. The C++ reader says "empty file (0 bytes)", and
  LithRez Studio's viewer shows that.
- `tron2-usa Game2.rez:TEX/PROTOTYPE/M06/DAN/FLOORANGLEUV.dtx` (64×1024,
  DXT5, 6 mips) and `GLASSARROWRUNNER.dtx` (32×512, DXT5, 5 mips) are
  missing their last level: 64 bytes for a 2×32 mip. Whatever wrote them
  sized levels in whole blocks, which is 0 bytes for a level 2 wide. The
  engine would fail to load them (`dtx_Create` checks the stream's error
  status). `read()` keeps the short level and sets `truncated`, so they
  still round-trip. `read(data, lenient=False)` raises instead.

## PS2 textures

The PS2 version of NOLF (both discs, `LITH_PS2.REZ`) holds 4,465 DTX
files per disc (six more files per disc got `.dtx` names when the
archive's hashed names were recovered, but are binary attribute files:
hash collisions). All are
version -5 and need nothing new to read: every one reads and writes back
byte for byte.

| | US + Europe |
|---|---|
| `BPP_32P` | 8,746 |
| `BPP_32` | 184 (63 with a leftover `PALLETE32` section, ignored) |
| mips | 3 in 7,804, 1 in 1,097, 4 in 27 |
| sections | only `PALLETE32`; header bytes zero or MSVC's 0xCD fill |

**Where they come from.** DEdit's *Convert to 32P* (Build 69
`DEdit/TextureDlg.cpp` `DoConvertTo32P`, `Misc/TextureHelper.cpp`
`Write8BitDTX`, `shared/world/LTTexture.cpp`) made them, and the PS2 files
fit it exactly:

- the palette is the 256 most used colors of the 32-bit image, sorted by
  use (`Convert32BitTo8BitPalletized`). Other pixels get the nearest entry
  by `NearestColor`, which never updates its best distance and so returns
  the last entry for every color it can't match exactly;
- each entry is B, G, R, **A / 2** (`m_color.a/2;//128`): opaque is stored
  as 0x7f. The PS2's GS takes 0x80 as opaque;
- three mips (`CreateMipMaps(3)`), point sampled from the top level;
- the flags are cut to `FULLBRITE`, `SECTIONSFIXED` and `32BITSYSCOPY`;
  `m_Extra` (except the format), the user flags and the command string come
  from the texture it replaced, and `m_Extra[1]` is set to the mip count.

Of the palettes, 6,420 reach exactly 0x7f and 1,520 are all zero (made
from textures whose alpha was unused and left 0).

**Channel order.** B, G, R, A, as on the PC, with no CLUT swizzle: of 600
PS2 textures compared with their PC twins at the same mip size, 589 match
best that way (mean error 6.7 per channel, against 21.7 with the GS's CSM1
bit swap and 34 with R and B exchanged). The rest are textures that
changed between the versions.

**Alpha.** The PC engine draws palette alpha as stored (`CC_32PtoBF`
copies it), so the nine PS2-made `BPP_32P` files in the PC games (NOLF
GOTY's `GL0007`, NOLF2's German menu sprites) are drawn half see-through
there, glass included. On the PS2 the same bytes are opaque. Against the
PC twins, PS2 alpha × 2 is the PC alpha (127 → 254, 98 → 196). So there
are two readings of one format, and the file can't say which applies:

- *stored* (`decode()`, the default; C++ `PaletteAlpha::Stored`): the PC's;
- *PS2* (`decode(ps2_alpha=True)`, `ps2_palette_alpha()`;
  `PaletteAlpha::Ps2`): the inverse of DEdit's halving. 0x7f and up is
  255, below that a × 255 / 127 rounded, and an all-zero palette is
  opaque. Only two PS2 files (`l1btn`, `r1btn`) have entries above 0x80.

`Texture.ps2_style_palette` (no entry above 0x80) marks DEdit's output; it
is true of the nine PC files too. LithRez Studio's texture view has a "PS2
alpha" switch for textures from a PS2 archive. The Blender importer's
*Auto* setting reads PS2 alpha when the palette is DEdit-style.

**Writing.** `from_rgba(..., bpp=BPP_32P)` writes as DEdit did (three
mips, its flags, alpha halved with `ps2_alpha`), but with a real palette:
exact when the image has 256 colors or fewer (most used first), else a
median cut in RGBA with each pixel mapped to the nearest entry. Lower mips
are box filtered and mapped onto the same palette. `encode_32p(palette=,
stored_palette=)` maps an image onto an existing palette and keeps its
stored bytes. Every PS2 32P texture's image maps back onto its own
palette exactly (`tests/roundtrip_ps2_dtx.py`).

## Alpha

Many skins use alpha as a mask rather than as transparency. For example,
AvP2 Primal Hunt's `MERC7_HEAD` is DXT3 with every alpha 0. Importers
shouldn't wire alpha to transparency without the game's render style or
command string to say so.

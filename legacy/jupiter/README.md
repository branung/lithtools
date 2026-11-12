# legacy/jupiter/: the LithTech Jupiter tools, from Build 69's source

The tools for NOLF2, Contract J.A.C.K. and Tron 2.0: DEdit, ModelEdit,
WinPacker and WorldPacker, Model_Packer, LithRez, FxED, the render-style
tools, the 3ds Max and Maya plug-ins and the rest, built from the source
in Touchdown Entertainment's 2006 "Jupiter System Enterprise Edition,
Build 69" (archive.org `Lithtech-Jupiter-System-Enterprise-Edition-Build-69`;
`../../INVENTORY.md`).

The first goal is a working build of that source, for testing under Wine;
a byte-matching build of Contract J.A.C.K.'s October 2003 tools comes
later (`../../INVENTORY.md`, `../../FORMATS.md`).

## src/

`src/` is the setup's on-disk layout, `Engine/` and `Libs/` side by side, so
every project's relative paths resolve as Monolith wrote them. It was
copied out of the package by `../../scripts/import_b69.py`, which records what it
takes and leaves out, and committed unmodified apart from line endings (LF
for text files). Every later change is a commit of its own.

Not in the repository:

- **The plug-in SDKs** (`Engine/tools/Plugins/SDKFiles`: the 3ds Max 7, Maya
  6.5 and 7.0, and Photoshop SDKs that Build 69 carries). They are
  Autodesk's and Adobe's, and Monolith's own readme says they are not
  redistributable. The build copies them in from the Build 69 package.
  The Max 3-6 and Maya 3.0-6.0 SDKs that the other plug-in projects name are
  not in the package at all.
- **The DirectX SDK** the tools build against (February 2006), likewise
  taken from the package (`DirectX/dxsdk_feb2006.exe`).
- Build outputs that the package happened to carry (`Libs/built`, the Maya
  plug-ins' `Release_Maya70` folders, `.obj`/`.exp` files) and the
  Microsoft runtime DLLs in `tools/MiscPack`.

## Building

```sh
legacy/jupiter/build/build.sh                 # everything, in the cjacksrc-build image
legacy/jupiter/build/build.sh DEdit LithRez   # just these (and what they need)
legacy/jupiter/build/build.sh --force         # from scratch (headers are not tracked)
python3 legacy/jupiter/build/package.py legacy/jupiter/src extracted/jupiter-b69/x/Tools/Tools \
        out/dist/lithtools-jupiter     # the distribution
legacy/jupiter/build/build.sh --shell         # then: legacy/jupiter/build/smoke.sh out/dist/lithtools-jupiter
```

Needs: the `cjacksrc-build` Docker image (Ubuntu 24.04 with 32-bit Wine),
VC .NET 2003 (`msvc-7.10-3077`; by default the repository's toolchains
submodule, `ext/toolchains/msvc-7.10-3077`, override with `VC71`),
the Build 69 package unpacked by file group (`extracted/jupiter-b69/x`,
`B69`) and its DirectX SDK unpacked to `extracted/dxsdk-feb2006`
(`Include/`, `Lib/x86/`; `DXSDK`). `../../INVENTORY.md` says where each comes
from. Outputs go where the projects put them (`src/Engine/built`,
`src/Libs/built`, and the Maya 7.0 projects' `Release Maya70` folders),
all gitignored.

`build/vcbuild.py` does what devenv would: it reads `Tools.sln` and the
`.vcproj` files and runs cl/rc/lib/link 13.10/7.10 under Wine, each
project in the configuration the solution maps it to, after its
dependencies. Makefile projects that call `devenv X.sln /build "CONFIG"`
(the MFC-DLL libraries, the plug-ins) are built by building that solution
configuration. Every command's response file is kept beside its output.
`build/build.sh` builds `Tools.sln` with DEdit in "Release Public
Development" (the public builds' configuration) and without the
plug-in projects for 3ds Max 3-6 and Maya 4.0-6.0, whose SDKs are not in
Build 69, then the Photoshop plug-in's own solution.

`build/package.py` lays the result out as Build 69's Tools folder, with its
data files and Microsoft runtime DLLs and a MANIFEST.txt of where every
file came from; `build/dist-README.txt` becomes its README.txt.
`build/smoke.sh` starts each GUI tool on a virtual display and records
whether it stays up, its windows and a screenshot (`out/smoke/`).

## Changes to Build 69's source

Each is its own commit; changes to code are marked `lithtools:` in it:

- `tools/DEdit/dedit60.vcproj`: the "Release Public Development"
  configuration no longer links `d3drm.lib` (Direct3D Retained Mode, gone
  from the DirectX 9 SDK; nothing in DEdit uses it).
- `tools/Model_Packer`: writes model file version 23 by default, without
  the OBB radius that version 24 added, as NOLF2, Contract J.A.C.K. and
  Tron 2.0 expect (`../../FORMATS.md`); `-ModelVersion 25` writes Build 69's
  format. Its output is then byte-identical to Contract J.A.C.K.'s
  Model_Packer for the CJ toolkit's sample character, with and without an
  OBB (and with `-ModelVersion 25`, to Build 69's).

## Terms

`src/` is Monolith Productions' / LithTech, Inc.'s code (later Touchdown
Entertainment); `src/Engine/sdk/Legal.txt` is its notice. archive.org's
description says Touchdown released it under the GPL, but nothing in the
package says so, and no licence file is in it. Treat it as nolf2src's
`docs/LICENSE_NOTES.md` treats the same tree: work out the terms before
distributing anything built from it. Third-party code inside it keeps its
own terms (zlib, FreeType, ImageMagick, STLport, S3TC's `s3tc.lib`, ...).

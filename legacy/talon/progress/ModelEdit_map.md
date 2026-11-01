# ModelEdit.exe (AvP2 Tools Update 1/2, "Talon"): the map of the image

"ModelEdit for Talon", built 2001-12-19 (Update 1's and Update 2's are the
same file). It loads and saves `.abc` models (v12, the version NOLF and AvP2
ship: `../../FORMATS.md`), imports `.lta`, and draws with OpenGL. 1,945 rows of
the function table in its own code, 0x00401000..0x00437170, 1,927 once the
library header functions (`ModelEdit-screen-exclude.csv`) are screened out.

## What built it

The Rich header: 73 C++ objects of cl 12.00.9044 (0x31, VC6 SP5 with the
Processor Pack: `toolchains/msvc-6.00-9044`), 86 C++ objects of 8569 (MFC
4.2's, nafxcw.lib), the C runtime's and C++ library's (180 C and 23 C++ of
8047, 40 masm 7299, 3 AliasObj; and 6 C++ and 1 C of 8168, 1 C of 8447:
our link of the same toolchain's libraries has the same rows), cvtres 1735,
link 6.00. No row of 8966: the 73 objects of 9044 are all Monolith's, the
LT2 libraries' included.

- **cl 9044, `/O1`**: functions are unaligned (0x40101d, 0x401020, ...) and
  have the /O1 shapes; Build 69's sources compiled so match 1,001 of the
  image's functions as they stand (identical, `make progress`).
- **Literals in `.data`, pooled across objects**: probably `/Gf`, not
  `/GF` (read-only) or none: "*.abc" (0x48927c), "abc" (0x48dc50) and the
  model filter (0x48dc58) are one copy each, used by AddChildModelDlg,
  ModelEditDlg and SocketEdit. (The second "AmbientLight" is
  CommandStringDlg's key table, char arrays, not a literal.) Our build
  doesn't pool yet; it moves data, not code.
- **No `/GR`**: the only Monolith class with a type descriptor is StdLith's
  CLithException (thrown); the rest are MFC's, VC6's C++ library's and the
  old iostreams' (169 type descriptors: 135 `.?AV` classes, 23 `.?AU`
  structs, 11 `.PA` pointers for catch clauses). `/GX`: C++ EH frames.
- **Static MFC and C runtime**: nafxcw.lib (from 0x00437194, wincore.obj),
  libcmt.lib, **VC6's own C++ library** (libcpmt.lib: `std::` iostreams,
  locale, string; no STLport, whose `_STL` namespace isn't there) and the
  **old iostreams** (libcimt.lib: `ofstream`, `filebuf`, 0x0045b8a7..),
  found with `tools/libscan.py` and msvc-6.00-9044's libraries.
- **The LT2 libraries**: RegMgr (0x00436870..0x00436be2) and StdLith
  (0x00436bf0..0x00437164: l_allocator, dynarray, helpers, and
  lithexception, whose CLithException has the type descriptor) from `src/LT2/lithshared`, compiled with 9044. StdLith is
  the **allocator-override build** (`STDLITH_ALLOC_OVERRIDE`, the
  `StdLith_alloc.lib` stdlith.h names): l_allocator.obj's code runs from
  LAllocCount::ClearCounts (0x436cf0) straight on to LAllocCount::Alloc
  (0x436d10); DefStdlithAlloc and DefStdlithFree (0x436c30, 0x436c50) come
  first in l_allocator, StdLith's own in 2001. No ButeMgr,
  DibMgr or zlib: libscan finds none of their functions, and nothing of
  theirs is named.
- **Imports**: OPENGL32 and GLU32 (the renderer), WINMM (timeGetTime),
  comctl32, comdlg32, the OLE DLLs (MFC's), no SHELL32 (Build 69's
  DirDialog, SHBrowseForFolder, is not there) and no Direct3D.

`src/LT2/tools/ModelEdit/ModelEdit.dsw` builds it so: build.mk's
ENV_/CL_/LINK_ModelEdit (and LT2_StdLith_alloc, LT2_RegMgr_Talon) are
msvc-6.00-9044's, with its headers, MFC and runtime. Its `std::string` is
compiled from Dinkumware's patched `<xstring>` (25 May 1998, no reference
counting; Jupiter Build 69 ships it in WONAPI's VC_STL_Header_Patches),
`src/LT2/tools/ModelEdit/stlpatch/` (string `_Tidy` 0x409e80); the rest of
the C++ library is 9044's. Every module that includes precompile.h opens
with `<xlocale>`'s ctype<unsigned short>::id initializer: 2001's
precompile.h included `<string>` (AboutDlg, model.cpp and transformmaker
have none).

## Layout (libscan.py)

| Range | What | Source |
|---|---|---|
| 0x00401000..0x00436849 | ModelEdit's own objects, in module order (below), with the template instances (CMoArray, `std::`) they emit | to reconstruct |
| 0x00436870..0x00436bf0 | RegMgr | lithshared/regmgr, regmgr60_Talon.dsp (cl 9044) |
| 0x00436bf0..0x00437170 | StdLith, allocator-override build | lithshared/stdlith, Lib_StdLith_alloc.dsp (cl 9044) |
| 0x00437170..0x00437194 | MFC's inline functions (AfxGetMainWnd, `??_9` vcall thunks) | the toolchain's |
| 0x00437194..0x0043ebac | nafxcw.lib (wincore.obj first) | the toolchain's |
| 0x0043ebac..0x0044ebea | libcmt.lib | the toolchain's |
| 0x0044ed78..0x0045b800 | libcpmt.lib (iostream.obj first) | the toolchain's |
| 0x0045b8a7..0x0045c6c4 | libcimt.lib (old iostreams) | the toolchain's |
| 0x0045fe9c.. | more libcmt, then nafxcw (dlgfile.obj ..) | the toolchain's |

Inside the own range, 36 rows are header functions of VC6's C++ library
(locale, facet, stream-buffer members: 0x409c14..0x411e5a, the iostream
code the LTA reader instantiates) and of MFC (CWnd::BeginModalState and
EndModalState, 0x401020, 0x40102c; CDumpContext's constructor) that
libscan finds byte-identical in the toolchain's libraries: screened out as
not Monolith's (`ModelEdit-screen-exclude.csv`).

## Modules

The link keeps the project's order, and it is the 2001 project's: **the
project's own folder in name order** (case-insensitive), with some shared
files listed in it (animtracker, the LTA importer, ltquatbase,
smmempnproc), then **the shared engine and model code in name order**,
then the LT2 libraries. Most objects start with the same guarded
initializer, 21 bytes (`testb $1,0x49746e; ...; push $0x4117e2; call
atexit`: a header static every TU that includes the model headers has),
which marks 54 module starts; the rest are placed by Build 69's objects'
matches and the strings. Counts are `make progress`'s, with Build 69's
code as it stands.

| Range | Module | Source (under src/LT2) | Rows | Ident. | Near | Small | Todo |
|---|---|---|--:|--:|--:|--:|--:|
| 0x00401000..0x00401041 | AboutDlg | tools/ModelEdit/AboutDlg.cpp | 6 | 5 | 0 | 1 | 0 |
| 0x00401041..0x00401298 | AddChildModelDlg | tools/ModelEdit/AddChildModelDlg.cpp | 9 | 4 | 1 | 2 | 2 |
| 0x00401298..0x00401414 | AddSocketDlg | tools/ModelEdit/AddSocketDlg.cpp | 8 | 6 | 1 | 1 | 0 |
| 0x00401414..0x0040150c | AnimCleanupDlg | tools/ModelEdit/AnimCleanupDlg.cpp | 6 | 5 | 0 | 1 | 0 |
| 0x0040150c..0x00401604 | AnimFramerateDlg | tools/ModelEdit/AnimFramerateDlg.cpp | 6 | 5 | 0 | 1 | 0 |
| 0x00401604..0x00401625 | AnimMergeDlg | tools/ModelEdit/AnimMergeDlg.cpp | 1 | 1 | 0 | 0 | 0 |
| 0x00401625..0x0040169f | animnumberdlg | tools/ModelEdit/animnumberdlg.cpp | 4 | 3 | 0 | 1 | 0 |
| 0x0040169f..0x00401bfd | animtracker | tools/shared/model/animtracker.cpp | 13 | 2 | 11 | 0 | 0 |
| 0x00401bfd..0x00402080 | CommandStringDlg | tools/ModelEdit/CommandStringDlg.cpp | 13 | 6 | 3 | 3 | 1 |
| 0x00402080..0x0040210d | ContinuousDlg | tools/ModelEdit/ContinuousDlg.cpp | 5 | 4 | 0 | 1 | 0 |
| 0x0040210d..0x00402144 | (unnamed, 0x40210d) | -- | 2 | 1 | 1 | 0 | 0 |
| 0x00402144..0x0040243f | DimensionsDlg | tools/ModelEdit/DimensionsDlg.cpp | 8 | 4 | 3 | 1 | 0 |
| 0x0040243f..0x00402807 | DragList | tools/ModelEdit/DragList.cpp | 18 | 17 | 0 | 1 | 0 |
| 0x00402807..0x00402d3e | LTEulerAngles | tools/ModelEdit/LTEulerAngles.cpp (build 51's) | 4 | 3 | 1 | 0 | 0 |
| 0x00402d3e..0x004035cd | GenLodDlg | tools/ModelEdit/GenLodDlg.cpp | 20 | 18 | 1 | 1 | 0 |
| 0x004035cd..0x00406db3 | gl_modelrender | tools/ModelEdit/gl_modelrender.cpp | 85 | 50 | 27 | 7 | 1 |
| 0x00406db3..0x00406f6a | ImportAnimation | tools/ModelEdit/ImportAnimation.cpp | 10 | 3 | 3 | 3 | 1 |
| 0x00406f6a..0x00407048 | KeyframeStringDlg | tools/ModelEdit/KeyframeStringDlg.cpp | 6 | 5 | 0 | 1 | 0 |
| 0x00407048..0x0040715a | KeyframeTimeDlg | tools/ModelEdit/KeyframeTimeDlg.cpp | 7 | 6 | 0 | 1 | 0 |
| 0x0040715a..0x004093f8 | KeyframeWnd | tools/ModelEdit/KeyframeWnd.cpp | 50 | 25 | 20 | 3 | 2 |
| 0x004093f8..0x00409a3e | loadltadlg | tools/ModelEdit/loadltadlg.cpp | 35 | 22 | 2 | 11 | 0 |
| 0x00409a3e..0x00409b6c | LODEdit | tools/ModelEdit/LODEdit.cpp | 6 | 5 | 0 | 1 | 0 |
| 0x00409b6c..0x0040b173 | LTA reader A (2001; lost) | -- | 58 | 41 | 4 | 9 | 4 |
| 0x0040b173..0x0040b3de | LTA reader B (2001; lost) | -- | 15 | 2 | 1 | 9 | 3 |
| 0x0040b3de..0x0040bc0a | LTA lexer (2001; lost) | -- | 26 | 2 | 11 | 0 | 13 |
| 0x0040bc0a..0x0041215b | ltaModel | tools/shared/model/ltaModel.cpp | 174 | 90 | 64 | 15 | 5 |
| 0x0041215b..0x00412636 | ltaParseNode? (2001; lost) | -- | 22 | 5 | 17 | 0 | 0 |
| 0x00412636..0x00415aee | ltaScene | tools/shared/model/ltaScene.cpp | 75 | 50 | 17 | 8 | 0 |
| 0x00415aee..0x00415fc9 | ltquatbase | sdk/inc/ltquatbase.cpp | 11 | 5 | 2 | 2 | 2 |
| 0x00415fc9..0x004160b8 | ModelEdit | tools/ModelEdit/ModelEdit.cpp | 11 | 3 | 2 | 6 | 0 |
| 0x004160b8..0x00421612 | ModelEditDlg | tools/ModelEdit/ModelEditDlg.cpp | 276 | 85 | 164 | 17 | 10 |
| 0x00421612..0x00421bfc | ModelInfoDlg | tools/ModelEdit/ModelInfoDlg.cpp | 18 | 6 | 10 | 1 | 1 |
| 0x00421bfc..0x00421c95 | MyEdit | tools/ModelEdit/MyEdit.cpp | 6 | 4 | 0 | 2 | 0 |
| 0x00421c95..0x00424903 | newgenlod | tools/ModelEdit/newgenlod.cpp | 89 | 50 | 28 | 10 | 1 |
| 0x00424903..0x00424cfe | PieceInfoDlg? (2001; lost) | -- | 18 | 4 | 12 | 1 | 1 |
| 0x00424cfe..0x00424db5 | RenameDlg | tools/ModelEdit/RenameDlg.cpp | 5 | 4 | 0 | 1 | 0 |
| 0x00424db5..0x00424e8b | RenameNodeDlg | tools/ModelEdit/RenameNodeDlg.cpp | 5 | 4 | 0 | 1 | 0 |
| 0x00424e8b..0x00425fb1 | RenderWnd | tools/ModelEdit/RenderWnd.cpp | 44 | 21 | 18 | 3 | 2 |
| 0x00425fb1..0x004260a7 | RotationDlg | tools/ModelEdit/RotationDlg.cpp | 5 | 4 | 0 | 1 | 0 |
| 0x004260a7..0x004272bc | smmempnproc | tools/shared/model/smmempnproc.cpp (not built yet) | 49 | 13 | 21 | 13 | 2 |
| 0x004272bc..0x00427866 | SocketEdit | tools/ModelEdit/SocketEdit.cpp | 10 | 1 | 6 | 2 | 1 |
| 0x00427866..0x00427ad8 | SolidRectWnd | tools/ModelEdit/SolidRectWnd.cpp | 14 | 13 | 0 | 1 | 0 |
| 0x00427ad8..0x00427af9 | StdAfx | tools/ModelEdit/StdAfx.cpp | 1 | 1 | 0 | 0 | 0 |
| 0x00427af9..0x00427e3b | StringDlg | tools/ModelEdit/StringDlg.cpp | 8 | 7 | 0 | 1 | 0 |
| 0x00427e3b..0x00427eac | TimeOffsetDlg | tools/ModelEdit/TimeOffsetDlg.cpp | 4 | 3 | 0 | 1 | 0 |
| 0x00427eac..0x00427fa2 | TranslationDlg | tools/ModelEdit/TranslationDlg.cpp | 5 | 4 | 0 | 1 | 0 |
| 0x00427fa2..0x0042804b | UVImportDlg | tools/ModelEdit/UVImportDlg.cpp | 5 | 4 | 0 | 1 | 0 |
| 0x0042804b..0x00428572 | WeightEditDlg | tools/ModelEdit/WeightEditDlg.cpp | 14 | 7 | 6 | 1 | 0 |
| 0x00428572..0x004286b6 | WeightSetSelectDlg | tools/ModelEdit/WeightSetSelectDlg.cpp | 7 | 4 | 2 | 1 | 0 |
| 0x004286b6..0x00428866 | win_idle | tools/ModelEdit/win_idle.cpp | 10 | 10 | 0 | 0 | 0 |
| 0x00428866..0x00428b4c | conparse | tools/shared/engine/conparse.cpp | 8 | 8 | 0 | 0 | 0 |
| 0x00428b4c..0x00428be4 | (unnamed, 0x428b4c) | -- | 7 | 0 | 2 | 3 | 2 |
| 0x00428be4..0x0042916e | dtxmgr | tools/shared/engine/dtxmgr.cpp | 10 | 8 | 2 | 0 | 0 |
| 0x0042916e..0x004292e7 | genltstream | tools/shared/engine/genltstream.cpp | 3 | 3 | 0 | 0 | 0 |
| 0x004292e7..0x00429636 | geomroutines | tools/shared/engine/geomroutines.cpp | 4 | 4 | 0 | 0 | 0 |
| 0x00429636..0x0042971f | (unnamed, 0x429636) | -- | 3 | 1 | 2 | 0 | 0 |
| 0x0042971f..0x0042f746 | model | tools/shared/model/model.cpp | 284 | 118 | 142 | 24 | 0 |
| 0x0042f746..0x0042fd4d | model_cleanup | tools/shared/model/model_cleanup.cpp | 14 | 6 | 7 | 1 | 0 |
| 0x0042fd4d..0x00431e78 | model_load (2001) | -- (none: 2001's .abc loader) | 48 | 13 | 31 | 0 | 4 |
| 0x00431e78..0x0043208c | model_ops | tools/shared/model/model_ops.cpp | 4 | 1 | 3 | 0 | 0 |
| 0x0043208c..0x00432cbb | model_save | tools/shared/model/model_save.cpp | 19 | 2 | 14 | 0 | 3 |
| 0x00432cbb..0x00433084 | modelallocations | tools/shared/model/modelallocations.cpp | 6 | 2 | 4 | 0 | 0 |
| 0x00433084..0x00435b46 | pixelformat | tools/shared/engine/pixelformat.cpp | 77 | 70 | 3 | 2 | 2 |
| 0x00435b46..0x00435b98 | simple_dalloc | tools/shared/engine/simple_dalloc.cpp | 3 | 2 | 0 | 1 | 0 |
| 0x00435b98..0x00435f3d | streamsim | tools/shared/engine/streamsim.cpp | 23 | 22 | 0 | 1 | 0 |
| 0x00435f3d..0x00436849 | transformmaker | tools/shared/model/transformmaker.cpp | 10 | 3 | 7 | 0 | 0 |
| 0x00436849..0x00436870 | (unnamed, 0x436849) | -- | 1 | 1 | 0 | 0 | 0 |
| 0x00436870..0x00436bf0 | RegMgr (LT2) | lithshared/regmgr (regmgr60_Talon.dsp) | 8 | 8 | 0 | 0 | 0 |
| 0x00436bf0..0x00437170 | StdLith (LT2, allocator override) | lithshared/stdlith (Lib_StdLith_alloc.dsp) | 26 | 20 | 1 | 4 | 1 |

What the modules are, where Build 69 doesn't say:

- **AboutDlg** has no initializer (it includes no model header); the
  About box says "About ModelEdit for Talon".
- **AddChildModelDlg** is 2001's: a modal "Select child model(s)" dialog
  (dialog 181, constructed at 0x401062) with its own file filter ("ABC
  Files (*.abc)|..."), not Build 69's CFileDialog template.
- **AnimMergeDlg** (the "Mood" dialog) has only its initializer: its
  functions are folded into other dialogs' identical ones.
- **(0x40210d)**: one function, `new` of a 4-byte object holding its
  argument; between ContinuousDlg and DimensionsDlg by name.
- **LTEulerAngles** sits between DragList and GenLodDlg: its file was named
  for its place there (EulerAngles.cpp?); build 51's copy matches.
- **The LTA reader, 0x409b6c..0x40bc0a and 0x41215b..0x412636**: the 2001
  parse-node reader whose interface smmempnproc.cpp still names
  (ILTAStream, LTAStreamFrom_istream, CLTALexAnalizer::analize_stream with
  LEX_OTHER/PUSH/POP/STRING/EOF, CParseNode with append/pop/isAtom/
  isString/getValue/deleteChildren, BuildParseTree, PrettyPrint). Three
  modules before ltaModel: A (0x409b6c, mostly the `std::` iostream and
  locale code it instantiates first), B (0x40b173, a three-guard
  initializer) and the lexer (0x40b3de: "unterminated or too-large string",
  "undefined character scanned"); one after it (0x41215b..0x41266d,
  CParseNode). No source of any of it survives: Build 69 replaced it with
  ltamgr's CLTANode. Written from the image (2026-10-30): cstream.cpp
  (0x40210d, LTAStreamFrom_istream), ltaBuildTree.cpp (A: a stand-in;
  BuildParseTree and PrettyPrint were dead-stripped, so it only
  instantiates the stream code A placed), ltaFindPN.cpp (B), ltalex.cpp,
  ltaParseNode.cpp; the file names are ours. ltaScene starts at
  0x41266d.
- **ltaModel, ltaScene**: the importer, written against the parse nodes
  (Build 69's is against CLTANode); the same commands and messages.
- **ModelEditDlg** is 2001's main dialog: list views for nodes, pieces and
  animations (no tree control: Build 69's ltwintree* classes aren't there),
  anim active-flag and keyframe windows, RectTracker, the registry key
  "Monolith Productions\Talon ModelEdit\1.0" (Build 69: "LithTech
  Inc.\Jupiter"), .abc open and save.
- **PieceInfoDlg? (0x424903)**: the "Piece Info" dialog (specular power and
  scale, texture index), Build 69's PieceMaterialDlg's forebear, between
  newgenlod and RenameDlg by name. Not taken: Build 69's is a different
  dialog.
- **smmempnproc** (0x4260a7) is in, with the `std::map<string,
  CLTACommand*>` code of its command table, built on the 2001 reader.
- **StdAfx** (0x427ad8) is its initializer alone.
- **(0x428b4c, 0x429636, 0x436849)**: 0x428b4c is the final-build LT
  counter (counter.cpp, between conparse and dtxmgr); 0x429636 is
  model.cpp's start (_TransformsLtoG, _TransformsGtoL,
  DefaultLoadChildFn); 0x436849 is unnamed. 0x428866, before conparse, is
  bdefs.cpp's module, `<xlocale>`'s initializer alone (ModelEdit has its
  own bdefs.cpp, including `<string>`).
- **LVEditList (0x415ee9..0x415fc9)**: a CListCtrl that starts label
  editing on F2, between ltquatbase and ModelEdit by name (ours); CDragList
  and the main dialog's piece and node lists derive from it.
- **model_load (0x42fd4d..0x431e78)**: 2001's `.abc` loader (the chunk names
  "Header", "Pieces", "Nodes", "ChildModels", "Animation", "Sockets",
  "AnimBindings"; "Model file is incorrect version"); Build 69's tools
  only read LTA. `../nolfsrc`'s decompiled engine (cmodelload.cpp) is the
  2000-era reader of the same format, a reading aid.
- **model_save (0x43208c..0x432cbb)**: 2001's `.abc` writer (the same
  chunk names); Build 69's model_save.cpp keeps only pieces of it.
- **transformmaker** runs to 0x436849: 2001's has more functions than
  Build 69's (0x43645f and 0x43661f, which call its others).

## Strings, by function (build/strmap.py)

Address order, three-character noise left out.

```
0x00401116    330  ABC Files (*.abc)|*.abc|LTA Files (*.lta)|*.lta|All Files (*.*)|*.*|| | *.abc | %s; %s
0x00401a34    217  LTAnim_End
0x00401e31    581  AmbientLight
0x0040236c    211  %f
0x00402f16    237  LOD: %d, Dist: %.1f, %% Triangles: %.0f
0x004036ef    183  OEM:\t\t%s  \n  Version:\t%s\n  Renderer:\t%s\n
0x0040477b    640  Tris: %d, Ticks: %d
0x00405bc3    296  333?OEM:\t\t%s  \n  Version:\t%s\n  Renderer:\t%s\n
0x004086d3    177  Clip Animation | Cannot clip past the last keyframe | Confirm Clip | Are you sure you want to clip the animation past the current keyframe?
0x00408784    179  Clip Animation | Cannot clip before the first keyframe | Confirm Clip | Are you sure you want to clip the animation before the current keyfram
0x00408837     68  Tag Keyframe | Cannot tag the first nor the last keyframe in an animation
0x0040887b    626  Tag Keyframe | Must have at least 2 untagged keyframes | Cannot tag the first nor the last keyframe in an animation
0x00408bef    194  Edit Keyframe Time | Cannot edit keyframe time of first keyframe
0x00408cb1    189  Edit Keyframe Time | Cannot edit keyframe time of first keyframe | A keyframe already exists with that time value.  Not changing keyframe
0x00408da8    240  Delete Keyframe | Cannot delete one of last two keyframes
0x004095a5    493  Unable to open file: %s | Error
0x00409956    126  Loading: %s
0x0040a286    220  missing locale facet
0x0040a6fc    202  %p
0x0040ae31    196  missing locale facet
0x0040b067     73  false | true
0x0040b410    258  unterminated or too-large string | undefined character scanned
0x0040b8ec    200  missing locale facet
0x0040bcba    674  error opening file  | loading lta file\r\n | on-load-cmds |  error Reading in LTA file  | generating normals.\r\n | --- done --- \r\n
0x0040c28e    283  error  | \r\n
0x0040c3b4   1164  Applying Deformers\r\n | ApplyDeformers Failed | Building LOD\r\n | BuildLOD Failed | Apply Sockets\r\n | ApplyModifiers Failed | ChildModels:\r\n-...
0x0040c860    820  add-deformer | out of mem allocating weights ... 
0x0040cb94    734  target | influences | weightsets |  found deformer a null target! syntax error in file ?? |  CrateDeformersFromPN 
0x0040cfe7    702  set-repl-lod-original | piece-priorities | tri-% | dists | cmd_BuildLOD Error: BuildLODs failed  | cmd_BuildLOD | set-repl-lod-original nodes tri- ...
0x0040d339    279  add-sockets | socket | if(!pModel->m_Sockets.SetSizeInit4(numSocks, LTNULL, pModel->GetAlloc( |  LNew on ModelSocket FAILED 
0x0040d450    238  parent | quat |  error socket requires a name  | parse socket nodes
0x0040d53e   1170  add-childmodels | SELF | \tLoading Child Model : | \r\n | .lta | Error:  |  ChildModel Load Failed...\r\n | Error : 
0x0040d9de    229  filename | Warning: child model node missing filename ... skipping\r\n | save-index | scale-skel
0x0040dac3    453  add-childmodels | SELF | \tAdding Child Model Reference: | \r\n
0x0040dc88    342  anim-weightsets | add-animweightsets | name | weights
0x0040ddde    464  anim-bindings | name | dims | translation | interp-time | weight-set
0x0040dfae    159  set-node-flags
0x0040e04d     70  set-command-string
0x0040e093     70  set-global-radius
0x0040e0d9    976  Creating Model \r\n |  out of mem ...  | CRITICAL ERROR | Creating Pieces \r\n | error converting shape to piece  | ->NodeToModelNode error  | Crea...
0x0040e716    311  null
0x0040e933    368  AnimSetToModelAnim | could not alloc keys 
0x0040eafc   1062  m_KeyFrames.SetSize2 failed ...   |  could not find piece for anim node.  | ModelNodeToAnimNode | could not find anim for  |  in AnimSet :  | inval...
0x0040efd7    669  could not open file  | Opening ChildModel | Error : Out of memory, failed new in ltaLoadChildModel | ltaLoadChildModel |  error Reading in LTA file...
0x0040f274    636  deformers\r\n | Apply Sockets\r\n | AnimBindings\r\n | Set Weights Sets\r\n | ApplyModifiers Failed | command string\r\n
0x00411a16    196  missing locale facet
0x00412923   1924  parent | geometry | mesh | vertex | normals | nrm-fs | tri-fs | tex-fs | WARNING :  |  has no texture coords/index\r\n | colors | appearance | mate...
0x004132af    715  OUT OF MEMORY!!! ALLOC CAnimSet  | base | keyframe | AnimSetFromPN | anims
0x00413641    636  keyframe | parse node is not a keyframe node  | times | values
0x00413916   1475  keyframe | frames | parent | no frames in this anim node  | vertex |  this vertex animation has no target ..  | posquat |  this POS/QUAT animation ...
0x00414150     43  \nBA
0x00414298    552  hierarchy | this model has no hierarchy!!  |  processing Hierarchy  | node_root | children | Hier has no children  | root
0x004144c0    132  transform
0x00414544    674  matrix | children | null | shape | Shapes in hier not tested ...  | Parsing Hierarchy
0x0041614f    257  RectTracker
0x00416e6f    237  Error! | Failed to create thread to load LTA
0x00416f8c    229  %s\\%s | rb
0x004170d8    407  load error | Failed to load lta file | rb | File Error | Could not open file | error number %d on load 
0x0041746e     10  Attachments
0x00417478    472  Talon ModelEdit | Monolith Productions | rb | Render | bgColor
0x00417650     40  File Error | Model file is invalid
0x004176a3    141  .lta
0x0041786f    519  Render | Talon ModelEdit | Monolith Productions | CameraFollow | Wireframe | Dims | AnimBox | DrawSkeleton | DrawOriginalModel | DrawSockets | Draw...
0x00418002   1846  Nodes | Pieces | Name | Render Window | Could not create render window | Render | Talon ModelEdit | Monolith Productions | CameraFollow | Wireframe...
0x004188e2    496  OpenDir | Talon ModelEdit | Monolith Productions | Model | Model Files (*.abc)|*.abc|Model Files (*.lta)|*.lta|All Files (*.*)|*. | *.abc
0x00418ad2   3133  OpenDir | Talon ModelEdit | Monolith Productions | Model | Model Files (*.abc)|*.abc|Model Files (*.lta)|*.lta|All Files (*.*)|*. | *.abc | rb | Fi...
0x0041970f    181  File Error | Model file is invalid
0x00419a3b    396  .abc | .lta | wb | File Error | Could not open file for writing.  Check to see that the file is not re | Error saving.
0x00419bd3    548  OpenDir | Talon ModelEdit | Monolith Productions | Model | Model Files (*.abc)|*.abc|All Files (*.*)|*.*|| | *.abc
0x00419df7     70  Model Editor | Save changes before exiting?
0x0041a4bf    470  Delete Animation | Cannot remove all animations from a model file. | Are you sure you want to remove these animations from the model file?
0x0041a695    668  static_model | static_model%d
0x0041ae12     70  LOD: %d
0x0041ae58    535  Building LODs | Unable to build LOD's (% too small?)
0x0041b227    492  %d
0x0041b799    826  %f | Try again | No applicable animations selected. | Removed %d keys.  No more available keys found. | Removed %d keys.  Next removal at variance ...
0x0041bad3    406  Animation Length
0x0041bc69    379  Animation Interpolation
0x0041bf8b     97  rb
0x0041c184    455  ModelEditTexture | Attachments | Talon ModelEdit | Monolith Productions | rb | SocketModel
0x0041c4c8    387  %.3f
0x0041cc09    427  %s_copy
0x0041cdb4    303  Couldn't find node: '%s'. | ModelEdit | '%s' has been renamed to '%s'.
0x0041d059    366  Error pasting keyframe. | ModelEdit | Error copying frames.
0x0041d20c    697  OpenDir | Talon ModelEdit | Monolith Productions | Texture | Texture Files (*.dtx)|*.dtx|All Files (*.*)|*.*|| | *.dtx | ModelEdit | Unable to load...
0x0041d4c5    697  %s\\%s
0x0041d8c7    918  Socket%d
0x0041de95   1898  Rotation: %.1f | Translation: %.1f
0x0041e752    234  %.3f
0x0041e83c    541  Error | No model loaded. | Do you really want to do this? | Are you sure?  This could take a while.. | Results:\n\tOld Dimensions: %f\n\tNew Dimens...
0x0041ec5d   2131  Warning! | The LOD's didn't regenerate correctly, by the way.. | Try again | No pieces were found to merge. | Select a piece first
0x0041f4b0    128  Finished.\nRemoved %d vertices\nMerged %d vertices | Results | What's the big idea? | No pieces are loaded.
0x0041f89e    390  NONE
0x0041fe37    880  Attachments | Talon ModelEdit | Monolith Productions | load error | Failed to load lta file | rb
0x0042021c    241  Render | Talon ModelEdit | Monolith Productions
0x0042030d    166  Render | Talon ModelEdit | Monolith Productions | %f
0x004206c3    242  Render | Talon ModelEdit | Monolith Productions | bgColor
0x004217e0    124  \r\n
0x0042185c    144  Pieces: %d | Verts: %d | Tris: %d
0x0042195c    672  File Version : %d | Number of animations: %d | ------------------------------ |    %s | Average keyframes per animation: %d | Number of triangles: ...
0x00424c6f    104  * %s
0x004255bd     70  Could not create OpenGL context | Error
0x0042623f    227  secs : %f\r\nbyte cnt : %d\r\n
0x00426332    281  animset: | \r\n
0x0042645b    281  hierarchy: | \r\n
0x00426584    268  shape: | \r\n
0x00426690    297  animset | hierarchy | shape
0x004274f6    604  OpenDir | Talon ModelEdit | Monolith Productions | Attachment | Model | Model Files (*.abc)|*.abc|Model Files (*.lta)|*.lta|All Files (*.*)|*. | *.abc
0x004282d8    109  %.3f
0x0042871f     24  !!CWinIdle Warning!! Idle thread not shut down!\n
0x00428e4d    638  dtx_Create
0x0042bceb   1015  VisRadius | AmbientLight | DirLight | ShadowProjectLength | ShadowLightDist | ShadowSizeX | ShadowSizeY | ShadowCenterOffset | FadeSpriteSize | Nor...
0x0042c28e    412  Model already exists. | Child model limit reached. | Node trees differ at node %s. | SetupChildModelRelation returned LTFALSE. | Out of memory.
0x00430613    271  AnimBindings
0x00430722    229  Sockets
0x004308ab   2372  Pieces | Nodes | ChildModels | SELF | Animation
0x00431233     88  Header
0x00432286    232  AnimBindings
0x0043280b    655  Header | Pieces | Nodes | ChildModels | Animation | Sockets
0x00435b98     63  \r]C
0x00435d29     31  \r]C
0x00436870    249  Software | ware
```

## Build 69 and the 2001 image

Build 69 (2006) added, and the image hasn't: the D3D/LTB export
(ExportD3D_Dlg, the external packer, lta2ltb_d3d), LTA saving (ltaSave),
oriented bounding boxes (EditOBBDlg, COBBTracker), render styles and piece
LOD materials (PieceMaterialDlg), the piece LOD generator (PieceLODGen,
PieceLODGenDlg), custom LOD import (ImportLODDlg), the string-key import
window (ImportStringKeysDlg), invalid-dimension resolution
(InvalidAnimDimsDlg), the FOV dialog (SetFOVDlg), the tree control
(ltwintree*), the project-directory browser (DirDialog), socket scale and
node names in the socket dialog, TdGuard, SIMD-aligned transforms, the
weight set on AnimTimeRef, `bool` for the tracker's LTBOOL. Build 69's
CommandStringDlg has none of the 2001 model command keys (AmbientLight,
DirLight, DrawIndexedDist, FadeRange*, FadeSprite*, FOVXOffset, Lod*,
NoAnimation, NormalRef, Shadow*, SpecularEnable, VisRadius, which model.cpp
also parses).

The image has, and Build 69 hasn't: the parse-node LTA reader, `.abc`
loading (model_load) and saving, the Piece Info dialog, the 2001 child
model dialog, the Animation Cleanup dialog (AnimCleanupDlg is in Build 69's
tree but not its project), the old iostreams, and the LT2 SDK's types (the
tracker's string-key callback with an append string and a replace flag,
the weight set per frame locator). The resource ids are 2001's own (dialog
198 is the Animation Cleanup dialog, Build 69's IDD_EXPORT_D3D): the
project's ModelEdit.rc is the image's, numbered, and resource.h is still
Build 69's.

Build 69's later code that its ModelEditDlg and CRenderWnd called
(DirDialog, EditOBBDlg, ExportD3D_Dlg, ImportLODDlg, ImportStringKeysDlg,
InvalidAnimDimsDlg, ltaSave, the ltwintree classes, PieceLODGen,
PieceLODGenDlg, PieceMaterialDlg, PieceMerge, SetFOVDlg; debug, estring
and exportsys, called by nothing) left the project once those were
brought back to 2001 (2026-10-30), and with them Build 69's LTA library.

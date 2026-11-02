# Bugs in the retail tools

Bugs in the Talon-era tools as they shipped (AvP2 v1.0's and Tools
Update 2's, NOLF Tools 001's), found while rebuilding them. Each is a
mistake in the code that a source change would fix. This is a living list:
add a bug when it is found, in the format below, whether or not it is
fixed yet.

Our sources reproduce every one of them, on purpose: compiling to the
retail machine code means compiling its mistakes too. The tools are to be
re-implemented later with these fixes applied (unlike the games, which fix
theirs in a modern build of the same sources); this list is what that work
starts from. An entry that is fixed then says so under its heading, with
where the fix is.

Every binary here is a reconstruction (`docs/PLAN.md`), so an entry is
read off the retail bytes at the address it names, never guessed, and says
how well our function matches the retail one there (`tools/exematch.py`,
`tools/fndiff.py`): identical, or near with the bug reproduced. Addresses
are as in the retail images (`build/reference.sh` extracts them into
`out/reference/`).

## Format

```
### <What goes wrong, as a level designer or a programmer would put it>

<binary> <address> (<function>, <source file>; identical or near). What the
code does, and what it should do; how it shows, if it does.

Not fixed yet.
```

## The bugs

### The Processor's ray selection count covers only the last world model

AvP2's Processor.exe 0x4450c0 (DoRaySelect,
src/LT2/tools/PreProcessor/RaySelecter.cpp; near: size-exact, one
multiply's operand order differs). Each RaySelecter object casts a ray at
every world model's polies and selects the brush of the last poly it hits.
The count of selections is cleared at the start of each world model
(0x4450e7) instead of once before the loop, so the function returns how
many rays selected something in the last world model only. Every selection
is still made; only the count is wrong, and it goes no further than the
log's "RaySelect: Did %d ray selections" (Processing, 0x440400).

Not fixed yet.

### Merging terrain surfaces skips a poly after each one it takes out

AvP2's Processor.exe 0x445b80 (MergeSurfaces_RS, the name ours,
src/LT2/tools/PreProcessor/RaySelecter.cpp; near: the same instructions,
one register spilled). MakeTerrain calls it (0x446714) to give polies with
the same surface one surface: the polies are hashed into 512 lists, and
each list's poly at i is taken out (0x445c67) and compared with the polies
left. Taking poly i out moves the next one into slot i, and the loop then
goes on to i + 1, so that poly is never compared as the first of a group.
Polies with the same surface can keep surfaces of their own: the terrain
keeps more surfaces than it needs. Nothing breaks.

Not fixed yet.

### A DTX texture with 9 to 15 mipmaps overflows the heap

AvP2's Processor.exe 0x40d380 (dtx_Create, the 2001 dtxmgr,
src/LT2/tools/PreProcessor/EditBrush2001.cpp; identical). The header check
accepts up to 15 mipmaps, but TextureData (0x18c bytes) has only 8 mip
records, so a texture claiming 9 to 15 makes dtx_Alloc and the mip reads
write past the end of the block. Build 69's dtxmgr and the engine's copy
have the same check. Only a malformed texture shows it.

Not fixed yet.

### Loading a brush with more than 65535 points or polies never ends

AvP2's Processor.exe 0x40dae3 and 0x40dbcc (CEditBrush::LoadTBW,
EditBrush2001.cpp; identical). The point and poly counters are WORDs, so
for a count over 65535 the loops wrap round, never finish and write past
the arrays. Only a malformed or very large .ed file shows it.

Not fixed yet.

### A brush that fails to load leaves garbage pointers to be deleted

AvP2's Processor.exe 0x40da20 (CEditBrush::LoadTBW, EditBrush2001.cpp;
identical). When a poly fails to load, the m_Polies slots after it are
left uninitialised (BaseNew<CEditPoly*>, 0x40f600, doesn't clear them),
and the caller (CEditRegion::LoadFile, 0x410870) has already put the brush
in the region. Term then deletes every non-null slot, garbage included. A
corrupt or truncated .ed file shows it.

Not fixed yet.

### A failed .ed load leaks the brushes loaded so far

AvP2's Processor.exe 0x410570 (CEditRegion::LoadFile,
src/LT2/tools/PreProcessor/EditRegion2001.cpp; identical). Brushes go into
m_Brushes and the brush indexer before the node tree links them, but the
region deletes brushes only through the tree (Term's
m_Brushes.RemoveAll() frees only the list's links). When a brush fails to
load (0x410879) it and every brush before it leak; when the node tree
fails or an exception is caught, every brush not yet in the tree does;
and the indexer stays allocated until the next load. Nothing shows: the
Processor stops after a failed load.

Not fixed yet.

### A texture name longer than 100 characters is lost

AvP2's Processor.exe 0x40fc08 (LoadEditPolyTBW,
src/LT2/tools/PreProcessor/EditObjects2001.cpp; identical). A poly's
texture name over 100 characters is replaced with an empty string instead
of being rejected or kept, so the poly silently loses its texture.

Not fixed yet.

### Making the visibility BSP carries on after it fails

AvP2's Processor.exe 0x40ce37 (MakeVisBSP, the name ours,
src/LT2/tools/PreProcessor/MakeBsps.cpp; identical). The result of the
second MakeBsp (after removing unseen geometry) is ignored, so a failed
visibility BSP goes on into the PVS; the physics BSP's path checks the
same call. 0x40cec7 also ignores SetupVisBSPLeaves' result (MakeWorldPVS
calls it again and checks).

Not fixed yet.

### Filtering a poly into the leaves leaks the pieces of a failed split

AvP2's Processor.exe 0x40cb61 (FilterPolyIntoLeaves_R, the name ours,
MakeBsps.cpp; near: one inlined dot product). When SplitPoly gives up, the
function returns without freeing the two pieces SplitPoly already made.
A memory leak during processing; nothing shows.

Not fixed yet.

### Splitting a poly list keeps polies SplitPoly gave up on

AvP2's Processor.exe 0x40c5fb (SplitPolyList, the name ours, MakeBsps.cpp;
identical). SplitPoly's result is ignored, so a poly it gave up on (more
than two edge splits) still goes on, in whatever pieces it left.

Not fixed yet.

### Terrain blockers free malloc'd pieces with delete

AvP2's Processor.exe 0x40c209 (AddTerrainBlockers, the name ours,
MakeBsps.cpp; near: the inline budget at two list destructors). The split
pieces were allocated with malloc and are freed with scalar delete; it
works only because the runtime's operator delete calls free.

Not fixed yet.

### Terrain occluder polies are unlinked but never freed

AvP2's Processor.exe 0x40bad0 (RemoveTerrainOccluderPolies, the name
ours, MakeBsps.cpp; identical). The polies are taken out of the world's
list and left allocated: a leak during processing; nothing shows.

Not fixed yet.

### The vis leak file name is appended with the whole buffer's size

AvP2's Processor.exe 0x40cd6b (MakeVisBSP, MakeBsps.cpp; identical).
`strncat(buf, "_Vis.lek", 270)` passes the size of the buffer rather than
the room left in it. Latent: the 255-character limit on the leak file
name keeps it inside the buffer.

Not fixed yet.

### Yes/no questions return MessageBox's code unchanged

AvP2's Processor.exe 0x430510 (AskQuestion,
src/LT2/tools/PreProcessor/ProcessorApp.cpp; identical). There is no
return after the IDYES, IDNO and IDCANCEL checks, so any other result
falls out with MessageBox's own code. Latent: with MB_YESNO and
MB_YESNOCANCEL nothing else comes back. (VC6 warns, C4715.)

Not fixed yet.

### The status window can stop updating once it fills

AvP2's Processor.exe 0x430220 (DrawStatusTextToScreen, ProcessorApp.cpp;
identical). Each line is put at the caret after selecting from
GetLimitText() instead of the text's length, so once the edit control
reaches its text limit, new status text may no longer show (read from the
window messages, not run). The log file is unaffected.

Not fixed yet.

### Two NULL checks are missing in the application

AvP2's Processor.exe 0x42fdb1 (CPreProcessorDlg::OnInitDialog) and
0x430640 (CPreProcessorThread::InitInstance), ProcessorApp.cpp; identical.
The AppWizard's NULL check on GetSystemMenu was removed, and the thread
calls g_pAppDlg->GetDlgItem without checking g_pAppDlg (its other branch
does). Latent: both are always there when these run.

Not fixed yet.

### Saving the DAT file can go on with an unset result after "No"

AvP2's Processor.exe 0x44170d (SaveDATFile,
src/LT2/tools/PreProcessor/Processing.cpp; identical). Answering No to
"Error opening output file. Try again?" leaves the result variable
unset, so the function returns whatever was on the stack; when that is
nonzero, it goes on to remove the existing DAT file and rename a temp
file that was never created.

Not fixed yet.

### NOLF's processor reports a DAT file it never wrote after "No"

NOLF's Processor.exe 0x44de92 (SaveDATFile,
src/LT2/tools/PreProcessorNOLF/Processing.cpp; near). Answering No to
"Error opening output file.  Try again?" returns TRUE, so DoProcessing
and ImportGeometry carry on and print the world's statistics as if the
DAT file had been written. With -skipdialog nobody can answer the
question, and the run waits for good.

Not fixed yet.

### NOLF's unseen-poly removal tests an unsigned root for "< 0"

NOLF's Processor.exe 0x45ad80 (CUnseenPolyRemover::RemoveUnseenPolies,
src/LT2/tools/PreProcessorNOLF/VisLeaves.cpp; identical). It returns
early if the world's root node is less than 0, but the root is unsigned
(`test; jae`), so the test never fires. Harmless; AvP2's version
(0x4498d0) has no test.

Not fixed yet.

### NOLF's processor never exits with -skipdialog

NOLF's Processor.exe (CProcessingDlg, the progress dialog,
src/LT2/tools/PreProcessorNOLF/ProcessorApp.cpp). With -skipdialog in
processor.opt the options dialog is skipped, but the progress dialog stays
up when processing is done, or has failed, until someone presses OK, so a
batch run of several worlds can't follow one with the next. AvP2's
processor closes it.

Not fixed yet.

### SaveDATFile's rename error messages are garbled

AvP2's Processor.exe 0x4415a0 and 0x4417c5 (SaveDATFile, Processing.cpp;
identical). The message buffer is filled only when the remove fails, so a
failed rename after a successful remove shows stack garbage at the start
of its message; and the message is formatted into itself, so each retry
prepends the previous one.

Not fixed yet.

### Surfaces made invisible by invisible.dtx still get lightmaps

AvP2's Processor.exe 0x43fa90 (GenerateSurfaces, Processing.cpp;
identical). Surfaces forced invisible because their texture is
textures\invisible.dtx keep SURF_LIGHTMAP, so lightmaps are made for
surfaces nobody sees (Build 69 clears the flag): wasted lightmap space in
the DAT.

Not fixed yet.

### The AI-volumes path takes the maximum lightmap size from the grid size

AvP2's Processor.exe 0x440e80 (DoProcessing, Processing.cpp; identical).
On the -aivolumes path, m_MaxLMSize is set from the DAT file's lightmap
grid size rather than its maximum lightmap size. Read from the code, not
confirmed by a run.

Not fixed yet.

### Light animations with too many frames, polies or vertices corrupt the DAT

AvP2's Processor.exe 0x432a40 (CPreMainWorld::SaveLightAnims,
src/LT2/tools/PreProcessor/PreWorld.cpp; near: two scheduling
differences). Each animation's frame count is written as a byte, its poly
count as a WORD and each poly's vertex count as a byte, while the data
behind them is written in full. Over 255 frames or 65535 polies makes the
file unreadable for LoadLightAnims; over 255 vertices loses colours.
Latent with normal levels.

Not fixed yet.

### The lightmap compression ratio divides by zero without light animations

AvP2's Processor.exe 0x433520 (CPreMainWorld::CalcLMCompressionRatio,
PreWorld.cpp; near, size-exact). The compressed size isn't checked for
zero, so a world with no light-animation data gets 0/0 (NaN) as
m_LMCompressionRatio in the statistics it prints.

Not fixed yet.

### Texture lights leak on failure, and divide by zero on a perpendicular plane

AvP2's Processor.exe 0x419e8d and 0x41a60b (CLightMapMaker::
GetTextureLights, src/LT2/tools/PreProcessor/LightMapMaker_r3.cpp; near).
When GetTextureInfo fails, the function returns without freeing what it
already put in its texture info list (a leak, only on the "Unable to get
texture lights" path). And if a texture's plane is perpendicular to its
poly, the tiling divides by zero (N . vNormal); the point then fails
InsideConvex, so no light is made for that texture there.

Not fixed yet.

### Moving a lightmap sample out of a wall never tests one corner

AvP2's Processor.exe 0x41ca1a..0x41ca9f (CLightMapMaker::
CalcSamplePositions, LightMapMaker_r3.cpp; near). The ring search for a
clear supersample stops both row searches before x = gx + r and both
column searches before y = gy + r, so the corner (gx + r, gy + r) is never
tried: a sample in a wall can move to a slightly farther clear point than
the nearest.

Not fixed yet.

### Tracing a light ray has a dead test and no stack bound

AvP2's Processor.exe 0x4181f8 (TraceSegment,
src/LT2/tools/PreProcessor/LightMapMaker_r2.cpp; near: an unused frame
slot). It compares the low WORD of the split node's CPrePoly* with
0xFFFF, a leftover of a WORD poly index: a pointer never has that low
word, so the test is always true (a node with no poly would crash). And
it pushes onto its 1024-entry stack without a bound, so a BSP deeper than
1024 splits along one ray would write past it. Both latent.

Not fixed yet.

### Unclipped lights give shadow-map animations colour instead of a mask

AvP2's Processor.exe 0x417180 (CLightMapMaker::ApplyLightToPoly,
LightMapMaker_r2.cpp; near). With clipping off, globally or for the
light, shadow-map animations get the light's normal colour from
CalcLightSample instead of the 255/0 mask, and the directional cone test
is skipped, so those shadow maps show graded colour where every other
light gives a full on/off mask. Read from the code, not confirmed by a
run.

Not fixed yet.

### Labelled world nodes leak their labels

AvP2's Processor.exe 0x44e750 (CWorldNode::Term,
src/LT2/tools/shared/world/WorldNode.cpp; identical) and 0x44e5c0 (its
destructor, which only calls Term). The 2001 Term never frees the node's
label, so every labelled node leaks its label string.

Not fixed yet.

### Opening a stream leaks the file handle when the stream can't be made

AvP2's Processor.exe 0x445910 (streamsim_Open,
src/LT2/tools/shared/engine/streamsim.cpp; identical). If `new SSFile`
fails, it returns NULL without closing the file it has just opened.

Not fixed yet.

### Console commands past the 30th argument are dropped

AvP2's Processor.exe 0x40a230 (cp_Parse,
src/LT2/tools/shared/engine/conparse.cpp; identical; Build 69 does the
same). At 30 arguments it stops and reports no more commands, silently
dropping everything after the 30th token, including any later
;-separated commands.

Not fixed yet.

### The physics BSP's T-junction splits keep uninitialised vertex colours

AvP2's Processor.exe 0x4475a0 (sub_4475a0, the T-junction split,
src/LT2/tools/PreProcessor/Terrain.cpp; identical). MakeBsp (0x43eea0)
sets every vertex colour of the BSP's polies to 255 (lighting does not
need them), and only then does MakeWorldModelBSP fix T-junctions
(0x43f810, FixTJunctions 0x4479c0). With -FixTJunc (on by default) a poly
split there gets two new polies from CreatePoly (malloc, nothing cleared)
that AddVert gives positions and normals but no colours, and the .dat
saves the colours as they are (CPreWorld::SaveFile, 0x436ad0, truncated
to bytes). So the physics BSP's split polies carry whatever the heap held:
the shipped exe's own output changes with nothing but the length of the
exe's name (Sample_DM.dat: 848 polies' colours), and a run slowed down
(by logging every allocation) differs the same way, so the timing of the
dialog thread's allocations counts too. Harmless as far as is known (the physics BSP
is not drawn), but it makes the .dat depend on the run.

Not fixed yet.

### The Max model exporter hangs on a skinned node whose first modifier isn't Physique or Skin

MaxModelExport30/40.dle 0x105b27d0 (ExportSkeletalPiece) and 0x105b3d70 /
0x105b4320 (ExportSkeletalBindings, 30 / 40), both in
src/Engine/tools/Plugins/MaxModelExport/Exporter.cpp; identical. Each
walks the node's derived-object chain with `while (obj &&
obj->SuperClassID() == GEN_DERIVOB_CLASS_ID)` looking for the Physique or
Skin modifier, but never advances `obj` inside the loop: if the first
derived object holds neither, the export loops forever. DeterminePieceType
does walk the chain (`GetObjRef()`), so a modifier further down marks the
piece skeletal and leads straight into the hang.

Not fixed yet.

### The Max model exporter writes a byte past the base texture path's buffer

MaxModelExport30/40.dle 0x105b4d50 / 0x105b5ad0 (GetTextureInfo,
src/Engine/tools/Plugins/MaxModelExport/Exporter.cpp; identical).
`new char[strlen(path)]` then `strcpy` into it: one byte short for the
terminator, a heap overrun on every texture path.

Not fixed yet.

### The Max model exporter samples vertex animations at the wrong times

MaxModelExport30/40.dle 0x105b1cd0 (ExportVertexAnimations,
src/Engine/tools/Plugins/MaxModelExport/Exporter.cpp; identical). The
start is computed in frames (`Start()/GetTicksPerFrame() + 1`) and then
used as ticks (`curTime = start + i*GetTicksPerFrame() + 1`), so unless
the animation range starts at 0 the samples are taken from the wrong
times.

Not fixed yet.

### The Max model exporter deletes a TriObject with delete instead of DeleteThis

MaxModelExport30/40.dle 0x105b1f50 (ExportVertexAnimation,
src/Engine/tools/Plugins/MaxModelExport/Exporter.cpp; identical). The
TriObject from ConvertToType is freed with `delete tri`, unlike every
other export path's `DeleteThis()`: across the plug-in/Max heap boundary
that can corrupt Max's heap.

Not fixed yet.

### The Max DTX loader reports success when it fails to store the pixels

MaxDTXLoader30/40.bmi 0x10002030 (DTXLoader::Load,
src/Engine/tools/Plugins/MaxDTXLoader/MaxDTXLoader.cpp; identical). When
`BitmapStorage::PutPixels` fails for a row, Load jumps to its cleanup and
returns NULL, but `*status` is still the BMMRES_SUCCESS it set on entry:
Max is told the load succeeded and handed no storage. Every other failure
path sets `*status` through ProcessImageIOError first.

Not fixed yet.

### Cancelling Add Child Model's Browse clears the typed names

ModelEdit.exe 0x401116 (CAddChildModelDlg::OnBrowse,
src/LT2/tools/ModelEdit/AddChildModelDlg.cpp; identical). If the file
dialog is cancelled, the edit field is still set from
`m_ofn.lpstrFileTitle`, which is empty, so whatever was typed is lost.
Joining the chosen names, `sprintf(szNames, "%s; %s", szNames, p)` reads
and writes the same buffer.

Not fixed yet.

### Several keyframe-window commands read past their arrays

ModelEdit.exe 0x407869 and 0x407bf1 (FindKeyframeX, GetNearestKeyframe,
src/LT2/tools/ModelEdit/KeyframeWnd.cpp; identical). Left of the first
keyframe FindKeyframeX returns -1, and GetNearestKeyframe reads
`m_XVal[-1]` (a selection box dragged from the far left). OnCreateKeyframe
(0x409283) reads `m_KeyFrames[n+1]` when the marker is on the last
keyframe, and OnDeleteTaggedKeyframes (0x408e98) and OnMouseMove read
`m_XVal[count]` when their loops run to the end.

Not fixed yet.

### The keyframe menu crashes with no animation loaded

ModelEdit.exe 0x4086d3, 0x408784, 0x408837, 0x408da8, 0x408e98 and
0x409283 (the clip, tag and create handlers,
src/LT2/tools/ModelEdit/KeyframeWnd.cpp; identical). They dereference
m_pModelAnim without a NULL check. Build 69 guards them with
CanPerformOperation.

Not fixed yet.

### A string of 2047 characters or a long symbol overruns the LTA lexer's buffer

ModelEdit.exe 0x40b410 (CLTALexAnalizer::analize_stream,
src/LT2/tools/shared/model/ltalex.cpp; near). A string of 2047 characters
or more reports both error messages and comes back as a symbol with the
output unchanged; symbols have no bounds check at all, so a long one
overruns the 2048-byte static buffer at 0x493f20.

Not fixed yet.

### Loading an LTA model leaks on a parse error

ModelEdit.exe 0x40bcba (ltaModelLoad) and 0x40efd7 (ltaLoadChildModel),
src/LT2/tools/shared/model/ltaModel.cpp (near; identical). On a parse
error ltaModelLoad leaks the model, the parse tree, the MetaModel and the
stream (and the stream and model too if either can't be allocated);
ltaLoadChildModel leaks the model.

Not fixed yet.

### LTA child models free a strdup'd name with delete

ModelEdit.exe 0x40d9d0 (ChildModel's destructor,
src/LT2/tools/shared/model/ltaModel.cpp; identical). The name is allocated
with strdup and freed with `delete`.

Not fixed yet.

### A weight set without a weights list crashes the LTA import

ModelEdit.exe 0x40dc88 (cmd_SetWeightSets,
src/LT2/tools/shared/model/ltaModel.cpp; identical). A weight set with no
`(weights ...)` list dereferences NULL, and one with no name is given
SetName(NULL).

Not fixed yet.

### The LTA import adds every socket in the file, not only add-sockets'

ModelEdit.exe 0x40d339 (cmd_ApplyAddSockets,
src/LT2/tools/shared/model/ltaModel.cpp; identical). It collects every
`socket` list anywhere in the parse tree, not just those under
`add-sockets`.

Not fixed yet.

### A failed LTA load goes on with a NULL model

ModelEdit.exe 0x417130 (CModelEditDlg::DoLoadModel,
src/LT2/tools/ModelEdit/ModelEditDlg.cpp; near). After "Failed to load lta
file" it carries on and dereferences the NULL model at 0x417326. Every
load also leaks the child-model remapping map it news for the load request
(0x417194): Model::Load never frees it either.

Not fixed yet.

### Add Child Model loads each child with the whole list as its name

ModelEdit.exe 0x41d57c (CModelEditDlg::OnAddChildModel,
src/LT2/tools/ModelEdit/ModelEditDlg.cpp; near). The name the child path
is built from is a copy of the whole "; "-separated list, not the current
token, so with more than one child each load is given a bad file name; on
the LTA path the name the messages print is never set.

Not fixed yet.

### Animation Cleanup misjudges every keyframe's position

ModelEdit.exe 0x41b692 (GetKeyframePercent,
src/LT2/tools/ModelEdit/ModelEditDlg.cpp; identical). It divides by `prev
- next` as an unsigned value, so the result is near 0, not the keyframe's
place between its neighbours, and the cleanup's error estimates are wrong.

Not fixed yet.

### Piece Info copies its changes one piece past the end

ModelEdit.exe 0x41eaf8 (CModelEditDlg::OnPieceInfo,
src/LT2/tools/ModelEdit/ModelEditDlg.cpp; near). The loop copying the
changes to the other selected pieces runs while `i <= NumPieces()`.

Not fixed yet.

### Model info shows each LOD's memory under the next LOD's heading

ModelEdit.exe 0x421bad (CModelInfoDlg::OnInitDialog,
src/LT2/tools/ModelEdit/ModelInfoDlg.cpp; identical). Under "LOD i+1" it
prints LOD i's memory: LOD 0 is counted twice and the last LOD never
shown.

Not fixed yet.

### The OpenGL error box has its text and title swapped

ModelEdit.exe 0x4255e6 (CRenderWnd::OnCreate,
src/LT2/tools/ModelEdit/RenderWnd.cpp; identical). The MessageBox's title
is "Could not create OpenGL context" and its text "Error". Build 69 has
the same.

Not fixed yet.

### Keyframe interpolation reads one keyframe past the end

ModelEdit.exe 0x429dd3 (AnimNode::InterpolateKeyFrame) and 0x42a213
(ModelAnim::InterpolateKeyFrame), src/LT2/tools/shared/model/model.cpp
(identical; the names are ours). The keyframe indices are checked with `>`
against NumKeyFrames(), so an index equal to the count passes and the
function reads and writes one keyframe past the array.

Not fixed yet.

### Model::Load reports success for a model it rejected

ModelEdit.exe 0x430914 and 0x430986 (Model::Load,
src/LT2/tools/shared/model/model_load.cpp; near). A header it rejects (a
bad version) returns 0, LT_OK, after Term(); a failed m_LODs.SetSize
returns 0 without calling Term(). On any error after the first child model
is allocated every ChildInfo leaks: the delete loop at 0x430f45 runs only
on success, and one whose own Load fails is never stored.

Not fixed yet.

### Right-clicking an empty part of a project bar list can crash

DEdit.exe 0x4016a8, 0x407c4a, 0x408465 and 0x40d3f0 (CModelDlg,
CSoundDlg, CSpriteDlg and CWorldsDlg::OnContextMenu,
src/LT2/tools/DEdit/ModelDlg.cpp, SoundDlg.cpp, SpriteDlg.cpp,
WorldsDlg.cpp; identical). Each loads its popup menu and takes its
first submenu, but when the click is not over an item it carries on
without returning, and TrackPopupMenu is called through the NULL submenu
pointer.

Not fixed yet.

### Create New Sprite never creates the sprite

DEdit.exe 0x403b2d (CProjectBar::OnPopupCreateNewSprite,
src/LT2/tools/DEdit/Controls/ProjectBar.cpp; identical). The flag that
says the name was accepted is never set TRUE: the test at 0x403c9c reads
a stack slot nothing in the function writes, so whether the sprite is made
depends on what was left there.

Not fixed yet.

### Importing textures ignores whether the copies worked

DEdit.exe 0x409a5e..0x409afb (CTextureDlg::DoImportOperation,
src/LT2/tools/DEdit/TextureDlg.cpp; identical). The results of copying
the imported images into the project are not checked: a failed copy goes
on as if it had worked.

Not fixed yet.

### A TGA that fails to load is reported as a PCX

DEdit.exe 0x40bddf (CTextureDlg::CopyImageToTexture,
src/LT2/tools/DEdit/TextureDlg.cpp; identical). When a .tga can't be
loaded the message shown is IDS_CANTLOADPCX.

Not fixed yet.

### The large texture preview never draws the texture's name

DEdit.exe 0x409601 (CTextureDlg::RenderLargeImage,
src/LT2/tools/DEdit/TextureDlg.cpp; near). The name is drawn only
when the dialog's m_hFont is set, and nothing ever sets it.

Not fixed yet.

### Importing an OBJ file's texture coordinates throws them away

DEdit.exe 0x412916 (CObjInterface::ImportObjTexCoords,
src/LT2/tools/DEdit/FileFormats/CObjInterface.cpp; identical). The `vt`
lines are parsed, and the values are discarded instead of stored.

Not fixed yet.

### IntersectRay's backface test is the reverse of Build 69's

DEdit.exe 0x40fdd9 (CBasePoly::IntersectRay,
src/LT2/tools/DEdit/World/BasePoly.cpp; identical). The bBackface test
is inverted against Build 69's: a ray from behind the poly is the one
accepted. Whether DEdit's 2001 callers rely on it has not been checked.

Not fixed yet.

### A self-closing dialog retries its timer for ever

DEdit.exe 0x414566 (CTimedCloseDlg::OnPaint, the names ours, a dialog of
an unknown module after ColumnCtrl's, src/LT2/tools/DEdit/Controls/ColumnCtrl.cpp
for now; identical). On
its first paint it sets its timer (id and elapse 0) and, when SetTimer
fails, jumps back and calls it again, with nothing to end the loop.

Not fixed yet.

### Vertices drawn in the parallel views get garbage depth

DEdit.exe 0x41a2f6 (D3DRender::DrawVert, src/LT2/tools/DEdit/draw/draw_d3d.cpp;
identical). In a parallel view the vertex's z and rhw are left
uninitialised, and the box is clipped against the y limit where the x
limit is meant.

Not fixed yet.

### Shading a textured poly can take the square root of a negative number

DEdit.exe 0x419a31 (D3DRender::DrawTexturedPoly,
src/LT2/tools/DEdit/draw/draw_d3d.cpp; near). The shade is made
from the square roots of the normal's components, which are negative for
half the polies: the colour is NaN.

Not fixed yet.

### Object dimensions keep the smaller box

DEdit.exe 0x41d6ea (DrawBase::DrawObject, src/LT2/tools/DEdit/draw/DrawBase.cpp;
identical). Combining an object's dims with its model's, the smaller
value is kept for each axis where the larger is meant. Build 69 has the
same.

Not fixed yet.

### New World can go ahead on an unset flag

DEdit.exe 0x4178b1 (CDEditApp::OnFileNewWorld, src/LT2/tools/DEdit/DEdit.cpp;
identical). The flag that decides whether the world is created is read
without being set on one path.

Not fixed yet.

### Closing a world saves it without asking

DEdit.exe 0x4179da (CDEditApp::OnFileCloseWorld, src/LT2/tools/DEdit/DEdit.cpp;
identical). Close World saves the world (CRegionDoc::SaveRegion) and
closes it, with no question whether to save the changes; closing the
window asks. It may be meant.

Not fixed yet.

### Making a poly's texture space orthogonal subtracts a number from Q

DEdit.exe 0x42555d (CEditPoly::SetTextureSpace,
src/LT2/tools/DEdit/World/EditPoly.cpp; near). When P and Q aren't
perpendicular, Q is to have its P component removed, `Q -= P * P.Dot(Q)`;
the code is `Q -= P.Dot(Q)`, subtracting the dot product from each of Q's
components (0x425611..0x425627), so Q comes out skewed. AvP2's
Processor.exe has the same code (0x41a610).

Not fixed yet.

### The main toolbar is titled "an unnamed file"

DEdit.exe 0x42e4ab (CMainFrame::OnCreate, src/LT2/tools/DEdit/Frames/MainFrm.cpp;
identical). The toolbar's title is loaded from string 61446 ("an unnamed
file") where 61448 ("Standard") is meant, and the status bar's panes are
sized from an empty string.

Not fixed yet.

### Run World's checks on the world's name don't work

DEdit.exe 0x43086b (CMainFrame::OnRunWorld, src/LT2/tools/DEdit/Frames/MainFrm.cpp;
identical). A path length is compared with itself, so the test is always
true; and the .dat name is made by dropping the last three characters of
the world's name, so a .lta world runs as "name..dat".

Not fixed yet.

### Building a rez file overwrites its first file's first four bytes

DEdit.exe 0x4312f2 (BuildRezFile, src/LT2/tools/DEdit/MakeRezFile.cpp;
identical). The directory's offset is written at the start of the file
after the first packed file has been written there, over its first four
bytes.

Not fixed yet.

### The default script editor is empty

DEdit.exe 0x42edff (CMainFrame::RestoreRegistryInfo,
src/LT2/tools/DEdit/Frames/MainFrm.cpp; identical). The default
is loaded from string 32797, which the image's string table hasn't: with
nothing in the registry the script editor is "".

Not fixed yet.

### Selecting a sprite frame removes it

DEdit.exe 0x42bd0f (CFrameList::OnSelchange,
src/LT2/tools/DEdit/Controls/FrameList.cpp; identical). The selection
change is handled as a double click, and the sprite editor's double-click
handler (0x460e06) removes the frame: selecting a frame deletes it.

Not fixed yet.

### Dropping on a list item appends "**" to its text

DEdit.exe 0x431d96 (CMyListCtrl::OnButtonUp, src/LT2/tools/DEdit/mlistctrl.cpp;
identical). Ending a drag over an item appends "**" to the item's
text.

Not fixed yet.

### The display options never read "Show surface color"

DEdit.exe 0x44e111 (CPropPageDisplay::OnInitDialog,
src/LT2/tools/DEdit/dlgs/PropPageDisplay.cpp; identical). The check box
is not set from the options, so OK saves it FALSE whatever it was.

Not fixed yet.

### The Direct3D options never save "blurry textures"

DEdit.exe 0x44e9ae (CPropPageOptionsD3D::UpdateDisplayOptions,
src/LT2/tools/DEdit/dlgs/PropPageOptionsD3D.cpp; identical). Every other
control is written back to the options; the blurry-textures check box is
not.

Not fixed yet.

### The primitive options report that saving failed

DEdit.exe 0x43fe4a (COptionsPrimitives::Save,
src/LT2/tools/DEdit/Options/OptionsPrimitives.cpp; identical). It saves
and returns FALSE.

Not fixed yet.

### The preferences' Browse has no file filter, and OK reads past a buffer

DEdit.exe 0x443b37 (CPreferencesDlg, src/LT2/tools/DEdit/Dlgs/PreferencesDlg.cpp;
identical). Browse loads its filter from string 32796, which the image
hasn't, so the file dialog has none; OnOK reads at most 256 characters
into a 512-byte buffer, so a longer path is cut short.

Not fixed yet.

### A region view's menu items are always enabled

DEdit.exe 0x455523 (CRegionView::EnableAMenuItem,
src/LT2/tools/DEdit/Views/RegionView.cpp; not matched yet: ours still has
Build 69's test). It calls EnableMenuItem with 0, MF_ENABLED|MF_BYCOMMAND,
whatever the values it is given: no item it is used for is ever greyed.

Not fixed yet.

### Select Detail Brushes leaves the wait cursor up

DEdit.exe 0x45ad15 (CRegionView::OnSelectDetailBrushes,
src/LT2/tools/DEdit/Views/RegionView.cpp; near). It calls
BeginWaitCursor and never EndWaitCursor.

Not fixed yet.

### Hide and Unhide Inverse crash with no region

DEdit.exe 0x45adff and 0x45ae4d (CRegionView's hide/unhide-inverse
handlers, src/LT2/tools/DEdit/Views/RegionView.cpp; identical). They
take the root node at the region pointer plus 0x18 without checking the
region: with none they read address 0x18.

Not fixed yet.

### Two trackers are named "OrbitVertex"

DEdit.exe 0x453cd7 and 0x453d78 (CRegionView::InitTracker,
src/LT2/tools/DEdit/Views/RegionView.cpp; identical). The left-click
light node selection tracker (OnSelectLightNode) is given the name of the
tracker before it, "OrbitVertex": a lookup by name finds the first.

Not fixed yet.

### Nearest-colour matching returns the last colour close enough

DEdit.exe 0x462154 (LTTexture's NearestColor,
src/LT2/tools/DEdit/Misc/Texture.cpp; identical). The best distance is
never updated, so every palette entry within 765 replaces the previous
choice: the result is the last such entry, not the nearest; alpha is
ignored.

Not fixed yet.

### Writing an 8-bit DTX leaks its stream and floods the debug output

DEdit.exe 0x4624a7 (Write8BitDTX, src/LT2/tools/DEdit/Misc/Texture.cpp;
identical). On failure the stream is not released, and every pixel
written is also sent to OutputDebugString.

Not fixed yet.

### Converting a texture's format can leave freed mipmaps in use

DEdit.exe 0x460311 (ConvertTextureData,
src/LT2/tools/DEdit/Misc/TextureHelper.cpp; identical). The old mipmaps
are freed for any target format but replaced only for 32-bit and
compressed ones, leaving the texture pointing at freed memory for the
others; the new data, from new[], is freed with delete.

Not fixed yet.

### A docking bar's non-client paint releases its DC twice

DEdit.exe 0x47130d (CMRCSizeControlBar::OnNcPaint,
src/LT2/tools/DEdit/MRCEXT/SIZECONT.CPP; identical). ReleaseDC is called
twice for the one GetWindowDC.

Not fixed yet.

### Two trackers read members their constructors never set

DEdit.exe 0x4756cf and 0x475df1 (CRVTrackerDrawPoly's constructor and the
reader of m_bAllowCancel, +0xcc, src/LT2/tools/DEdit/Trackers/RVTrackerDrawPoly.cpp;
identical) and 0x4773ae (CRVTrackerNavMove, +0xb8, read and negated,
src/LT2/tools/DEdit/Trackers/RVTrackerNavMove.cpp; near). Neither
member is initialised, so whether a poly drawing can be cancelled, and the
navigation's starting value, depend on what was in the heap.

Not fixed yet.

### ModelEdit's background colour never reaches full brightness

ModelEdit.exe 0x004062a4 (SetGLMBackgroundColor(GLM_CONTEXT, COLORREF),
src/LT2/tools/ModelEdit/gl_modelrender.cpp; identical). Each byte of the
COLORREF is turned into OpenGL's 0..1 by multiplying by 1/256 (the
0.00390625 at 0x004745dc), not dividing by 255, so 255 becomes 0.996: a
white background is drawn very slightly grey, and every colour a shade
darker than the one picked.

Not fixed yet.

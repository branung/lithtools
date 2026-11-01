# The plan

Seventeen shipped binaries, about 2.26 MB of their own code
(`progress/targets.py`). The order is by what each teaches the next and by
what the evidence allows.

## 1. The plug-ins (Update 2, cl 12.00.8966)

Near-identical to the LithTech Jupiter SDK build 51's plug-ins (same code
sizes; the DTX loaders and model exporters differ in a handful of bytes),
whose descendants' source is Build 69's. Start from that source, compiled
with VC6 SP5, and bring it back.

- [x] **IMFLithDTX.dll** (Maya DTX image plug-in): done. Build 69's source,
  build 51's libraries and SDK headers, no TdGuard, StdLith and MFC's
  import library linked: 105 functions identical, 81 small, none left.
- [x] **MaxDTXLoader30/40** (2026-10-29): every function identical in
  both (160, 167), Rich headers and resources too. Build 69's source
  without TdGuard or its MFC block; its defines kept; /YX, no /GF, no
  /G6; Lib_StdLith as IMFLithDTX has it.
- [x] **MaxModelExport30/40**, **MaxWorldExport30/40**,
  **MaxWorldImport30/40**, **MayaModelExport30/40**,
  **MayaWorldExport30/40** (2026-10-29): Build 69's source brought back,
  one source per plug-in for its 30 and 40 builds, with ltamgr (the LTA
  library they link, src/Engine/libs/ltamgr). The SDKs are in toolchains/
  (maxsdk-3.0; maxsdk-4.0, the 4.2 SDK with MAX_RELEASE 4000 and INT_PTR
  as int; maya-3.0-devkit, maya-4.0-devkit); Character Studio's headers
  are Build 69's (src/Engine/tools/Plugins/SDKFiles/CSTUDIO/SDK). Every
  function of the plug-ins' own objects and of ltamgr matches but three
  nears in MaxWorldExport (ExportNodeAsTriBrush, ExportNodeAsBrush: a cold
  block's placement and, in 40, a register; AddKeyFramerGroup: two loads,
  declaration count). What the 2002 builds were: VC6 SP5, STLport 4.0
  first in the includes (the LT2 copy, with threads and old iostreams:
  MSVCIRT.dll), /YX, no /GF (string literals in .data); /G6 for the model
  exporter only (it halves the world plug-ins' match). The scored
  ranges end at msvcprt's delop_s.obj (the runtime) and the linker's
  import-thunk runs are screened out: nothing is left unwritten. Left:
  the link's folding picks other copies of identical functions
  than Monolith's link, and import order differs; the assert strings name
  Monolith's paths (`..\sdkfiles\max30sdk\include\tab.h`,
  `C:\Jupiter_TO2_SDK\TO2_Build\...`): .data only. One stand-in (38
  typedefs, MaxModelExport30, guide 09's count); four retail bugs, in
  RETAIL_BUGS.md.

## 2. The world processors

No source of any version; their descendant is Build 69's PreProcessor
(the SDK build 51's Processor.exe is already the Jupiter one, writing world
version 85). `progress/Processor_avp2_map.md` is the map.

- [ ] **Processor.exe (AvP2)**, cl 12.00.8966, static MFC, 340 KB of code.
  - [x] Its libraries: StdLith, zlib 1.0.4 and RegMgr from
    `src/LT2/lithshared` compile identically to the AvP2 release's prebuilt
    libraries (153/153, 82/82, 14/14): /MT, no /Ob2 for StdLith and zlib.
  - [x] The project (`src/LT2/tools/PreProcessor/Processor.dsw`), with the
    shared engine and world code taken from Build 69 (about 130 of its
    functions match as Build 69 has them). In the default build since it
    links (2026-10-28).
  - [ ] Bring the shared classes back to 2001. Their class layouts come from
    the shipped vtables (tools/guides/07, `vtabaudit.py`, `vtabmap.py`): the
    LTA load/save methods Build 69 has are not in the 2001 classes, and
    removing a virtual changes every call through the vtable, so each class
    is laid out from the image, not trimmed by hand. Compiled against build
    51's SDK headers (src/Engine/sdk/inc, the tool code's TVector3) with the
    LT2 libraries.
  - [ ] The core, module by module in image order (the map). Its objects
    are scored before the image links (`partial_objs` in
    progress/targets.py).
    - [x] BasePoly, bindmgr (0x401000..0x402a70): matched.
    - [~] BrushToWorld (0x402a70..0x404bf0): the 2001 CSG version, written
      from the image. 62 of its 69 compiled functions identical; five near
      (inline budget, guide 09).
    - [~] BspGen (0x404c10..0x407e80): Build 69's brought back to 2001.
      120 of its 125 compiled functions identical; four near
      (GenerateBspTree, AddReversedPoly, SplitPoly,
      MinimizeNodePolySize).
    - [~] Hull making (BuildHulls.cpp, 0x408370..0x409f80): the
      portalization for the PVS, written from the image (no source
      survives). 54 of its 58 compiled functions identical; three near
      (MakeHulls, MakeHulls_R, CreatePoly<CPrePortal>).
    - [x] classbind, conparse (0x409f80..0x40a510): Build 69's brought
      back to 2001 (the 2001 tokenizer is rewritten from the image);
      matched but cp_Parse (loop layout).
    - [x] create_world_tree (0x40a510..0x40b540): the 2001 version, with
      terrain node sizes; matched. world_tree (0x44df60..) matched too,
      once its node lists lost the terrain one.
    - [x] FindWorldModel (0x411670..0x411f60): the 2001 version (world
      models from objects, surface flags from bool properties), written
      from the image; matched.
    - [x] genericprop_setup (0x411f60..0x4121c0): Build 69's brought back
      to 2001; matched but its static initializer (0x411f60..0x411fe0),
      which constructs an unused 12-byte global (0x484e04) from a header
      not yet found.
    - [x] gettextureflags (0x4131d0..0x413490): Build 69's brought back
      to 2001 (one texture a surface); matched. CPreMainWorld is laid out
      from its constructor (0x431060): 0xf8 bytes, with three virtuals
      for the DAT file not yet named.
    - [~] gettextureinfo (0x413490..0x413a10): Build 69's brought back to
      2001, with the LIGHTDEFS texture-light parser written from the
      image. Matched but ParseLightDefs, near by the operand order of one
      add in CMoArray::Insert2's inline (the declaration-count kind?
      dummy declarations, local order and names don't move it).
    - [~] LeafPortals (0x413a10..0x4144f0, the name ours): the portal
      helpers (ClonePortal, CopyPortal, the PVS's two-portal clipper,
      MakeBoxPortals, Flip, CalcPortalPlane), written from the image.
      Near: MakeBoxPortals (one SetVert inlined that shipped calls) and
      CalcPortalPlane (the operand order of commutative ops in the
      Newell sums; the source order doesn't move it).
    - [x] LeakFile (0x4144f0..0x414e40, the name ours): FindLeakPath and
      MakeLeakFile, written from the image; matched (but two CMoArray
      template instances). CWorldNode::GetPos is the 2001 accessor (not
      virtual, by reference, 0x44e8e0).
    - [x] lightmap_compress (0x414fb0..0x415200): the 2001 RLE (32-bit
      pixels, shadow-map runs), written from the image; matched.
    - [~] lightmap_planes (0x415200..0x415770, the names ours): the six
      lightmap planes, FindLMPlane and GetLMPlaneVectors, written from the
      image. Near: the static initializer, which builds each PVector
      argument in a temporary and copies it (ours constructs in place).
    - [~] LightMapMaker (0x415770..0x41e070): the 2001 lightmap maker,
      written from the image with Build 69's as a guide (it is a
      WorldTreeHelper and the ILTPreLight; per-world-model LMMWorlds in a
      WorldTree; threads; texture lights; light animations made by the
      objects' PreLight functions; light groups). 348 rows matched; about
      25 near (inline budget, operand order, registers and stack slots),
      listed by `build/modcheck.sh Processor-avp2 LightMapMaker 0x415770
      0x41e070`. Written in five parts in parallel, then folded together.
    - [x] ltquatbase (0x423dd0..0x4240e0): the SDK's source as it is;
      matched.
    - [~] mathlt_impl (0x424210..0x424b40): Build 69's brought back to 2001
      (ILTMath's own virtuals; the vector overloads; GetEulerAngles written
      from the image). Matched but GetEulerAngles: one temporary in another
      stack slot.
    - [x] Navigator (0x424b40..0x425490, DEdit/draw): Build 69's brought
      back to 2001 (no LTA; the look-at point saved; a description as its
      length and bytes; BOOL returns); matched. ("PNB" in the string map is
      the bytes of its vtable's first entry, 0x00424e50.)
    - [x] Node (0x425490..0x425520): Build 69's plus SetParentNodes; matched.
    - [x] node_ops (0x425520..0x425940): Build 69's, with no_DestroyNode
      taking the node off the region's lists itself; matched.
    - [x] parse_world_info (0x425940..0x425b10): Build 69's with
      LightTableRes and PBlockSize; matched. The physics module starts at
      0x425b10.
    - [~] PreBrush (0x42b8e0..0x42c9c0; the name ours: MakeTempBrush and
      its plane snapping), pregeometry, PreLeaf, PreLightMap, PrePoly
      (..0x42f580): written from the image and Build 69's; mostly matched,
      nears are MakeTempBrush, CalcSnappedPlane, CPreLeaf's ctor/dtor,
      RemoveCollinearVertices, MakePortal.
    - [~] PreWorld (0x430a40..0x43e5a0): Build 69's brought back to 2001
      (the DAT format, light anims, AI volumes, sub-BSPs, physics blocks),
      written in four parts in parallel and folded together. Mostly
      matched; near: CPreWorld::LoadFile and SaveFile (structure right),
      CPreMainWorld::LoadFile, SaveLightAnims, SetupPolyArrays,
      TermGeometry, InitPolyVertexIndices, MakePolyEdges and a few more.
      Not written: CPreSubBsp's constructor (0x433af0).
    - [~] ProblemBrushes (0x43e5a0..0x43e700, the name ours) and
      Processing (0x43e700..0x441cb0): matched but DoProcessing (the
      2001 CEditRegion is 0x128 bytes; ours still has the prefab
      manager), MakeWorldPVS, AddWorldModels, SaveDATFile; not written:
      GenerateSurfaces (needs the 2001 CEditPoly).
    - [~] The six modules after Processing (2026-10-26, six agents in
      parallel, one file each; 56.4% -> 62.4%): ProcessorOptions.cpp
      (the Processing Options dialog, 0x442e30..0x4445fa, 34/36),
      RaySelecter.cpp (ray selection, replacetextures, terrain surface
      merging, 7/9), Terrain.cpp (5/8), TJunctions.cpp (6/8),
      VisLeaves.cpp (unseen poly removal and the PVS's helpers, 20/22),
      WorldApprox.cpp (FastApprox and MakePVS, 16/18). Left to fold in,
      from their reports (all needed before the link, none changes a
      score today):
      - FixTJunctions returns void (0x4488c9); Processing.cpp says BOOL.
      - Stand-ins to move into the shared headers: CTJPrePoly::CalcExtents
        into CPrePoly, CTJPreSurface's subdivision test into CPreSurface,
        WA_LeafList (CPreLeafList has no declared constructor: it is
        built inline at 0x44b69f), CPrePortal::m_Unknown_3c is a PVector
        centre, CPreSurface::m_pBrush is a CEditPoly*, PreLeaf.h's
        m_Unknown_9c is a float.
      - VisLeaves names its functions by role, WorldApprox_decl.h by
        address; PVSInfo, PVSZone and LEAFF_FASTAPPROX are defined in
        both headers; g_Unknown_485adc is the flooded CPrePortal*.
      - CTJunctionFixer's arrays at 0x04 and 0x18 aren't the types
        Processing.cpp gives them (their element vtable is 0x47575c).
      - Function-table rows to split or drop: 0x444444 (inside
        0x4443d0), 0x445a3f (half of 0x445a30), 0x44626b (inside
        0x4461f0), 0x44a11c (half of 0x44a0f0), 0x44a200 (two functions,
        0x44a200 and 0x44a400).
      - streamsim.cpp (0x445910..0x445b75) doesn't compile only because
        genltstream.h declares WriteString(const char*) where the LT2
        SDK's ILTStream has a pure WriteString(char*).
      - Unwritten neighbours: Processing's template code at
        0x441ea0..0x442e10, MakePhysicsBlocks at 0x425d20 (Terrain.cpp
        calls it as MakePhysicsBlocks_425d20), CPreWorld::
        InitPolyVertexIndices (0x434170).
    - [~] Five more ranges (2026-10-28, five agents in parallel; 62.4% ->
      69.2%, unwritten 239 -> 115): MakeBsps.cpp (0x40b540.., 9/13),
      EditBrush2001.cpp (the 2001 dtxmgr and CEditBrush, all identical),
      EditObjects2001.cpp (the 2001 EditObjects and EditPoly, all
      identical), EditRegion2001.cpp (the 2001 CEditRegion, 0x128 bytes,
      and the .ed file, all identical), ProcessorApp.cpp (the MFC
      application and CPreSurface, all identical). To fold in, from
      their reports:
      - The 2001 classes into the shared files: dtxmgr (TextureData
        0x18c with an extra dword at 0xc4, 7-argument dtx_Alloc),
        EditVert.h (operator= by value, copying x, y, z), BasePoly.h (no
        virtual destructor; CopyAttributes the one virtual), EditPoly.h
        (CEditPoly 0x88 bytes, one texture), EditBrush (LoadTBW with
        three arguments), EditObjects.h (CBaseEditObj 0x88, five
        virtuals), WorldNode.h (the five-slot vtable), EditRegion (no
        m_PrefabMgr; LoadFile(CAbstractIO&, uint32*, CEditProjectMgr*)),
        PreSurface.h (its methods are in ProcessorApp.cpp now).
      - The stand-ins of the three 2001 files name the same things
        differently (CEditPoly2001, CEditBrush2001_ER, LoadEditPolyTBW
        and others; EditRegion2001_decl.h lists the pairs); g_DummyPos's
        initializer is written twice (EditBrush2001.cpp and
        EditObjects2001.cpp).
      - Processing.cpp should call ProcessorApp_GetTime (0x430210), not
        timeGetTime; CProcessorGlobs is 0x520 bytes, m_Unknown_4dc is
        m_bSkipDialog; PreWorld.cpp must not define the surface poly
        list functions now in ProcessorApp.cpp.
      - The application is probably three files (PreProcessor.cpp,
        PreProcessorDlg.cpp, PreSurface.cpp); MakeBsps.cpp declares
        BuildWorldHulls, MakeHullsAndRemoveUnseen, FixWorldTJunctions,
        SaveEDFile and MakeLeakFile locally, for a shared header.
      - Function-table rows that are not functions: 0x40d417, 0x40ddc2,
        0x40eefe, 0x40ef24, 0x40f489, 0x41096b, 0x410991, 0x430001,
        0x430046, 0x430080, 0x430082, 0x4305b6; 0x4317fa is inside a
        PreWorld function; 0x40e790 is two functions (0x40e790,
        0x40e7c0).
    - [x] The MFC application (0x42f580..0x430a40): ProcessorApp.cpp, above.
    - [~] The last unwritten rows (2026-10-28, seven agents; 69.2% ->
      73.9%, unwritten 115 -> 56): lighting in LightMapMaker_r1/r2/r3.cpp
      (rewrites of LightMapMaker.cpp's and LeafPortals.cpp's unmatched
      drafts, which they replace at the fold-in; g_LMPlanes'
      initializer, LightWorldModelCB and DoesSunLightHit identical, the
      rest near), PhysicsBlocks.cpp (the templates identical, the two
      makers near), PreWorld's and Processing's remaining functions
      (DoProcessing and GenerateSurfaces identical; ProblemBrushes.cpp
      gone, its two arrays are Processing's), streamsim (genltstream's
      WriteString takes char*), cp_Parse, genericprop_setup's
      initializer, the 2001 vis query in world_tree.cpp and CWorldNode::
      Term. What is left:
      - 22 written but near (the inline budget, registers, frame slots).
      - 9 kept from appearing by the inline budget: Processing's template
        code 0x441ea0..0x442ce0 (one inline level less than ours: depth 7
        reproduces it, or a derived element class, neither supported by
        the sources), TestBoxPlane 0x44dda0 (matches out of line once
        TestNode stops expanding it), FVec 0x41e090 (once
        ApplyLightToPoly calls it), the AddTail at 0x43d3a0 (PreWorld's).
      - 17 function-table rows that are pieces of other functions
        (0x413fcc 0x416567 0x41656c 0x416f66 0x417061 0x41706f 0x41746e
        0x417478 0x41756e 0x41c364 0x41de17 0x434443 0x434465 0x434470
        0x43a1d8 0x43d3c9 0x44dd8c) and 8 that aren't ours: StdLith
        starts at 0x44ea00 (0x44ea30 is l_allocator's), MFC at 0x456750
        (0x456750..0x456807). The scoreboard should drop them.
      - To link: the shared EditObjects, EditPlane, EditPoly, EditRegion
        and PrefabRef (Build 69's) don't compile; the 2001 files stand in
        for them (both fold-in lists above). world_tree.h needs
        VisQueryRequest and the two DoVisQuery virtuals; SaveNode belongs
        in a shared header; PBlockTable's first fields are the block
        size and the grid origin.
    - [x] The link (2026-10-28): Processor.exe links with nothing
      unresolved and nothing defined twice (no /FORCE), its sources in the
      image's module order. The 2001 files are folded into the shared ones
      (dtxmgr, EditBrush, EditObjects, EditPoly, EditRegion: the 2001 code
      and classes; EditPlane, PrefabMgr and PrefabRef are not in the image
      and left out of the project), LightMapMaker_r1/r2/r3 into
      LightMapMaker.cpp, LeafPortals.cpp and lightmap_planes.cpp in place
      of the drafts, the T-junction, PVS and vis-query stand-ins into
      TJunctions.h, VisLeaves.h and world_tree.h; bdefs.cpp (Build 69's) is
      the link's first object. Linked, the scorer verifies 47 more
      functions identical (most of them small ones, by relocation) and
      places all but 5 of ours. Left: the
      header declaration-count threshold (tools/guides/09) keeps SaveNode
      and PreSurfaceUser local to PreWorld.cpp, MakeBsps.cpp's
      declarations of Processing's functions local, and cost some Build 69
      declarations nothing links (dtx_Resize, dtx_Save, ..., EditBrush's
      IsOpen, ...).
    - [x] The resources (2026-10-28): PreProcessor.rc, resource.h and
      Res/PreProcessor.ico, decompiled with tools/rsrc2rc.py; all seven
      byte-identical in the linked exe (tools/rsrcdiff.py). RC is
      Developer Studio's (msvc-6.00-9782), VC6's own.
    - [ ] The near functions. A first parallel round (2026-10-28, five
      agents by source file) took 1357 -> 1376. What it found:
      - The inline budget is spent from the LAST call site backwards: the
        earliest sites are the ones refused. Inline calls that compile to
        nothing, added late, push early sites out of line (MakeHulls is
        identical with three; not kept, the real spelling is unknown).
        Among nested sites, each instance's first call is accepted before
        any second one (TestNode's 0x44dda0 can't be emitted that way).
      - CheckLeaves (0x4085a0) matches with a dead `if(0) free(&pLeaf)`
        that makes the leaf's stack store happen: a stand-in.
      - The declaration-count threshold, worked out (2026-10-28,
        tools/guides/09, "The declaration count is a symbol count, mod
        64"): what decides those functions is the number of symbols the
        front end has made before the point where the inlined function is
        first needed, modulo 64. With it both header changes are in:
        CPrePolyRef as a POD (-7 units, paid for with 57 units of Build 69
        declarations nothing uses: the light groups, CheckNodes,
        FindPolyByIndex, RemoveAllUnusedGeometry, GetWorldIndex, the two
        Collect*BlockerPolys), and CPreLeaf's 0x6c as CLeafListArray (+21,
        paid for by moving the portal functions to LeafPortals.h and the
        leaf searches to PreGeometry.h, and dropping CalcPortalPlane,
        which nothing defines). 0x42d060, 0x42d1c0, 0x42d370 and 0x43dad0
        identical, nothing lost (1390 -> 1394). Left: 0x423880 is
        unwritten again (unwritten 2 -> 3): it was paired with PreLeaf's
        BaseDelete<CPreLeafList*>, which the new class no longer emits
        out of line, as shipped doesn't; what belongs there is
        LightMapMaker's BaseDelete<CMoArray<uint32>>.
      - Pairings the scorer gets wrong: 0x42d370 (that destructor, not
        DeleteAndClearArray2, which shipped expands in CalculateLightFrame),
        0x423880 (BaseDelete<CMoArray<uint32>>, LightMapMaker's),
        0x4420b0/0x4420c0 (DefaultCache's Get/SetCacheSize), and
        0x441ea0..0x442ce0 are Processing.obj's template instances.
      - Round 2 (2026-10-28, five agents, plus one on the threshold):
        1376 -> 1390. Stand-ins (marked `talon: stand-in`, no behaviour
        change): MakeHulls' three dead GetSize calls, MakeLeafLists' two
        dead inline calls, Processing.cpp's closing `#pragma
        inline_depth(7)`, CheckLeaves' dead `if(0)` use. Measured: the
        number of `return`s (each one's destructor cleanup) and helper
        depth decide the inline budget more than call-site position; a
        VEC_* macro in place of an operator changes what inlines later;
        `#pragma inline_depth` only affects template instances compiled at
        the end of the TU.
      - Empty functions: ours folds every empty `ret` function into one
        (thd_Term, dsi_OnReturnError, CheckRegion, CPrePoly::Term, ...);
        the shipped link has at least two, CheckRegion's (0x40a0a0, with
        an empty function of BuildHulls', called at 0x40a018) apart from
        the rest (0x42dd40). Why the shipped linker kept them apart is not
        known yet. The scorer's verification of the 1-byte 0x42dd40 flips
        with it (accepted as lost in round 2): /OPT:ICF keeps the copy of
        a different object whenever the link's functions change (one
        unreferenced function added to Terrain.cpp moves it), the map
        names the group under that object, and the scorer can place by
        shape a copy the link dropped, which then can't be verified. With
        progress.py trying the copies our map names first, 0x42dd40 and
        0x40a0a0 are identical (1396).
      - Behaviour (2026-10-28): our exe processes all seven AvP2 v1.0
        sample worlds (Sample_DM, _Evac, _Evac_Events, _OverRun, _Marine,
        _Pred, _Alien; `Processor.exe <world.ed> -skipdialog`) to .dat
        files byte-identical to the shipped exe's. Two near functions
        behaved differently and are fixed: DoBrushesIntersect's edge normal
        and threshold, and MakePortal's rounding (Pt(0)'s last + through
        _CVector's constructor). The comparison needs the exe's name to be
        as long as the shipped one's: the physics BSP's T-junction splits
        write uninitialised vertex colours, which follow the heap
        (RETAIL_BUGS.md). The harness is in out/runtest/ (not in git).
    - [ ] The rest of the BSP modules, problem brushes and leak files
      (0x40b540..), DTX output,
    world properties and texture flags (0x411690..), lighting (0x415cb0..),
    physics and terrain (0x425d20..), the MFC application and its dialogs,
    the main processing driver and DAT output (0x42f900.., 0x43e7e0..).
- [ ] **Processor.exe (NOLF)**, 415 KB: 85% the same strings as AvP2's;
  after AvP2's, as its diff. `progress/Processor_nolf_map.md` is the map.
  - [x] What built it (2026-10-29): its own code is **Intel C/C++ 4.0**
    `/O1` (the Rich header's 82 objects without `@comp.id`; icl's frames,
    padding and `.data1`), but PreWorld.cpp, VC6 SP3 (8447) `/O1`; the LT2
    libraries, MFC (nafxcw.lib) and the C runtime VC6 SP3's, statically
    linked. build.mk: `ENV_ICL40`, the NOLF_* library projects, a per-source
    compiler (`CL_<project>__<source>`, tools/dsp2mk.py).
  - [x] The project (2026-10-29): `src/LT2/tools/PreProcessorNOLF/
    Processor.dsw`, a copy of AvP2's with the shared code in
    `src/LT2/tools/sharedNOLF`, in the image's module order. Links with
    nothing unresolved. Brought back so far: DAT version 66, no AI volumes,
    no light groups, NOLF's option names, its resources (decompiled). First
    score: 1,058 identical of 2,633 (34.8%).
  - [~] The modules, by source file (the map's table and differences).
    First round (2026-10-30, seven agents): NOLF's SDK and StdLith headers,
    /EHa, NOLF's lighting (vertex colours, 16-bit lightmaps), world format,
    staged processing, options dialog and tokenizer: 1,297 identical of
    2,587 (42.4%). The rest is mostly held by icl's no-inlining into
    EH-framed functions (the map's "How icl 4.0 compiled it"); still to
    write: EditBrush's splitters (0x412770..0x4154f0), Terrain and
    TJunctions from the image, CPreWorld::LoadFile's format, PreWorld's
    older STL.
  - [ ] Behaviour: `tests/processor-nolf.sh`, the six sample worlds of the
    Tools 001 with DEdit's default options and with lighting and full
    visibility (the shipped exe is deterministic on all twelve).

## 2b. The other tools (on the scoreboard since 2026-10-24)

- [~] **lithrez.exe** (LithRez 1.00, Nov-6-1998; AvP2 v1.0 and NOLF Tools
  001 ship the same file): Visual C++ 5.0 SP3 (`toolchains/msvc-5.00-7303`,
  fetched from archive.org; build.mk's VC5), `src/LT2/tools/LithRez`.
  lithrez.cpp and rezutils.cpp (the app), the 1998 RezMgr (rezmgr, rezhash,
  rezfile) and Lith (basehash, baselist, virtlist) libraries, written from
  the image with Build 69's 2004 sources as a guide. Links; 94.2% (all but
  CRezMgr::Compact matched; the rest of the count is unwind funclets and
  tiny functions to look at).
- [ ] **TProcessor.exe** (DirectEngine World Processor, 1998, NOLF Tools
  001): Visual C++ 5.0, 236 KB of code with zlib 1.0.4.
- [ ] **LithExport.dll**, **LandExport.dll** (Softimage|3D exporters, AvP2
  v1.0 and NOLF Tools 001; NOLF's LithExport a debug build) and
  **LithMayaExport.mll** (Maya, 1999): need those packages' SDKs of the time.

## 3. The editors (cl 12.00.9044, static MFC)

- [ ] **ModelEdit.exe** (221 KB of its own code, 1,927 functions;
  `progress/ModelEdit_map.md`).
  - [x] The project (`src/LT2/tools/ModelEdit/ModelEdit.dsw`, 2026-10-30):
    Build 69's ModelEdit and shared model code with the LT2 SDK, StdLith
    (its allocator-override build) and RegMgr, cl 9044 `/O1`, static MFC,
    in the image's module order; the resources decompiled from the image
    (byte-identical). Links with nothing unresolved, with two kinds of
    code the image hasn't: Build 69's later features that its ModelEditDlg
    calls (the "Build 69 only" group) and Build 69's LTA library
    (ME_LTAMgr.dsp, a stand-in for the lost 2001 parse-node reader). As
    it stands: 1,001 identical.
  - [~] Bring it back to 2001. First round (2026-10-30, seven agents by
    image range): 1,664 identical of 1,891 (85.1% of the bytes), from
    1,001. Written from the image: the 2001 LTA parse-node reader (lexer,
    parse nodes, finders; ltaBuildTree a stand-in for its dead-stripped
    module), LVEditList, the Piece Info dialog, the .abc loader
    (model_load), the callback list, the LT counter, the Command String
    and Add Child Model dialogs, Animation Cleanup and the renderer's
    subdivision. Back to 2001: model.h's layouts (LTBOOL throughout),
    ltaModel/ltaScene on the reader, the main dialog (layout, message map,
    list views, .abc open and save), the render window, the transform
    maker and the model writer. The "Build 69 only" files and ME_LTAMgr
    are gone. Found: Dinkumware's patched <xstring>
    (src/LT2/tools/ModelEdit/stlpatch), <string> through precompile.h.
    Stand-ins: ltaBuildTree.cpp, CModelEditDlg::OnPieceMerge (2259 bytes,
    to write).
  - [ ] What's left: OnPieceMerge; about 45 near (the main dialog's
    OnImport, DoLoadModel, OnInitDialog, OnMouseMove and Animation Cleanup;
    Model::Load and the LTA loader's ltaModelLoad, ParseTreeToModel and
    cmd_ApplyAddChildModels; the transform maker's blends; the lexer's
    analize_stream; ParseShapeNode); the guarded-atexit initializers at
    three module starts (0x40243f, 0x4160b8, 0x436849) our objects emit
    last; string pooling (probably /Gf).
- [ ] **DEdit.exe** (573 KB of code, 4,471 functions scored;
  `progress/DEdit_map.md`).
  - [x] The project (`src/LT2/tools/DEdit/DEdit.dsw`, 2026-10-30): Build
    69's DEdit with the processors' 2001 world headers (DEdit's own copies
    of the world sources in DEdit/World, Build 69's editor functions added;
    the editor's declarations under DIRECTEDITOR_BUILD), ModelEdit's 2001
    model code, the LT2 SDK, StdLith (the allocator-override build) and
    RegMgr, and the 2001 GenRegMgr, ButeMgr, CryptMgr, DibMgr, LTA library
    and zlib compiled with 9044 /O2; cl 9044 /O1 /Gf, static MFC, STLport,
    DirectDraw 4 / Direct3D 3 (DX 8.1 SDK headers); the resources
    decompiled from the image (all 224 byte-identical). Links with nothing
    unresolved, with Build 69's later features in a group of their own and
    stand-ins (the map's last section). As it stands: 2,499 identical
    (38.6% of the bytes); the libraries nearly all identical, model.cpp
    226 of 267.
  - [x] Bring it back to 2001, in nine parts by image range (near / todo
    counts as recorded; each part owns the headers of its modules):
    1. Project Bar group, 0x401000..0x40d69f (345 rows: 169 near, 4 todo).
    2. A-C, 0x40d69f..0x415075 (301: 90 near, 6 todo): AdvancedSelect ..
       CylinderPrimDlg, concommand, conparse, BasePoly.
    3. D, 0x415075..0x41fdf1 (286: 126 near, 10 todo): the application
       (DEdit.cpp, resource.h), the renderer (d3d_texturemgr, draw_d3d,
       DrawBase), dtxmgr, d_filemgr, dhashtable.
    4. E, 0x41fdf1..0x42b5ee (369: 139 near, 9 todo): the world core
       (EditBrush, EditObjects, EditPoly, EditRegion and the shared world
       headers, kept so the processors' objects don't change), the project
       manager, clipboard, helpers, the .map exporter (2001's, to write).
    5. F-N, 0x42b5ee..0x43c8de (661: 111 near, 13 todo): FilePalette ..
       MainFrm, the model dims reader (2001's), the model code (with
       ModelEdit), mtreectl, Navigator (shared with the processors),
       node_ops.
    6. N-P, 0x43c8de..0x44fbee (720: 220 near, 29 todo): the node view,
       object browser and importer, the options, pixelformat, the
       processing dialog, the property editors and pages, PropList.
    7. R, 0x44fbee..0x45f697 (481: 268 near, 13 todo): RegionDoc (and its
       light animations), RegionFrame, RegionView, the resource manager.
    8. S-W, 0x45f697..0x46f3c0 (402: 191 near, 11 todo): Splash ..
       WorldNode: textures (Texture.cpp with LTTexture's dumps), the view
       functions and primitives, undo, the vector controls.
    9. MRCEXT, the trackers and the libraries, 0x46f3c0..0x47d040,
       0x483020..0x48d740, 0x4a25f0..0x4a7d00 (804: 184 near, 20 todo).
    Then: the Build 69 only group and the stand-ins out; concommand and
    DebugDlg (2001's) into their places by name; zlib linked as a default
    library; LTTexture's code into Texture.cpp.
  - [~] First matching round (2026-10-30, nine agents by the parts
    above, then integrated): 3,855 identical of 4,402 (80.1% of the
    bytes), from 2,499. Links clean with no Build 69 only source left
    (the group and the rest: level errors, prefab manager and references,
    hot keys, model view, the later options pages, trackers with no 2001
    vtable, ...; removed). concommand and DebugDlg in their places,
    LTTexture's code in Texture.cpp. Written from the image: the Options
    Pages group, the 2001 tracker set-up (in RegionView.cpp, where the
    image has it), RegionView's world, OBJ and LTA imports and prefab
    save (.ed), the model dims reader, MakeRezFile, KeyMapDlg, mlistctrl,
    OptionsPrimitives, OptionsWarnings, PreferencesDlg, RezBuildDlg, the
    processor's world code under DIRECTEDITOR_BUILD with the world tree
    (the module the map had as unknown at 0x46db7a), the LTA library's
    2001 API. 117 interior function-table rows screened out and 13 missed
    starts added (progress/extent-splits.csv); the second scored range
    from CryptMgr's constructor, 0x4a25e0. 33 retail bugs (RETAIL_BUGS.md).
  - [ ] What's left: 209 near and 58 todo; two 2001-only modules not
    written: the 16-bit software renderer (0x429b55..0x42b077,
    dm_CreateNormalDrawMgr a stand-in) and the self-closing dialog after
    ColumnCtrl (0x414546, its handlers a stand-in in ColumnCtrl.cpp, its
    module unknown); the stand-ins (progress/DEdit_map.md's last section:
    Build 69's vertex flags and textured planes over the 2001 poly, the
    trackers' FlushTracker, ViewDef's camera settings, viewrender's clip
    mask and lighting, three light-texel functions of ViewFunctions, the
    Quick Help dialog); Build 69's prefab-node code in EditRegion; zlib
    and CryptMgr linked as default libraries (they are project
    dependencies still); tools/rsrc2rc.py's three fixes (TOOLBAR
    statements, cursor and icon order, WS_TABSTOP comboboxes).

## Method

As tools/guides/ describes: the shipped function's bytes (`exedump.py`),
our source, compiled one object at a time (`make -f build/build.mk <obj>`
in `make shell`), compared (`fndiff.py`, `exematch.py`), recorded
(`make progress-update`). Ghidra (`tools/ghidra/decomp.sh`) is a reading
aid, never source.

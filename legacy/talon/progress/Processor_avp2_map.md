# Processor.exe (AvP2 v1.0): the map of the image

Built 2001-10-03 with cl 12.00.8966; MFC 4.2 (nafxcw.lib, build 8569) and
the C runtime (libcmt.lib, libcimt.lib) linked statically; STLport (its
`_STL` exceptions are the only RTTI besides StdLith's CLithException
family: no /GR). 1,534 functions in its own code, 0x00401000..0x00456750, as the
enumeration counts them: 17 of them are interior blocks of others
(progress/Processor-avp2-screen-exclude.csv).

## Layout (libscan.py)

| Range | What | Source |
|---|---|---|
| 0x00401000..0x0044eab0 | Processor's own objects (with inline template instances of StdLith's CMoArray etc.) | to reconstruct |
| 0x0044eab0..0x00450ad0 | StdLith | src/LT2/lithshared/stdlith: identical to the AvP2 release's StdLith.lib |
| 0x00450ad0..0x004564d2 | zlib 1.0.4 | src/LT2/lithshared/zlib: identical |
| 0x004564e0..0x00456742 | RegMgr | src/LT2/lithshared/regmgr: identical |
| 0x00456750..0x0045680f | MFC's inline functions (CMemoryException's and CNotSupportedException's constructors and destructors, AfxGetMainWnd, `??_9` vcall thunks) | the toolchain's |
| 0x0045680f.. | MFC (nafxcw.lib), then the C runtime | the toolchain's |

## Build 69's code in it

Build 69's PreProcessor and shared tool sources (`../jupiter/src`),
compiled with VC6 8966 as they are, match about 130 distinct functions
(~20 KB), nearly all shared code: pixelformat (33 functions, 12 KB),
PropList (32), streamsim (16), geomroutines, Threads, dtxmgr, genltstream,
world_tree, light_table, WorldNode, BasePoly, Navigator, bindmgr. The core
-- BSP generation, hulls, the PVS, lighting, terrain, physics BSPs -- does
not: Build 69 rewrote it (only 24 of the image's 184 message strings are in
Build 69's sources, 18 of them in Processing.cpp).

## Modules, by the strings their functions use (build/strmap.py)

Address order; the strings name the module (tools/guides/13, step 1).

```
0x00402c90    896  Creating temp brushes | NoSnap | !! Empty world model BSP found (%s) | Finding intersecters | Sorting intersecters | Merging intersecters | Transferring inside fragments | %d BTW poly splits. | Num Polies: %d.  Joining polies... | %d polies removed. | %d polies left.
0x00404630    905  *** Bad poly found! > 2 edge splits! (%0.1f %0.1f %0.1f)
0x00404e20    656  BSP tree %0.2f%% balanced.
0x004054a0   2325  *** Bad poly found! > 2 edge splits! (%0.1f %0.1f %0.1f)
0x00407780    304  %d polies (%d nodes) added for lightmapping.
0x00407e80    936  *** Bad poly found! > 2 edge splits! (%0.1f %0.1f %0.1f)
0x004085c0   2680  *** Bad poly found! > 2 edge splits! (%0.1f %0.1f %0.1f)
0x00409a10    963  *** Bad poly found! > 2 edge splits! (%0.1f %0.1f %0.1f)
0x00409f80    172  ObjectDLLSetup
0x0040a510    128  TerrainNodeSize
0x0040a590    128  WorldNodeSize
0x0040b540   1392  Found %d problem brushes |   #%d : <Empty> |   #%d : %0.1f %0.1f %0.1f | Creating BSP. | Making PhysicsBSP leak file. | Removing unseen geometry. | Recreating BSP. | Physics precalculation... | -  %s (%d polies)
0x0040bb50    160  --------------- | Selecting %d problem brushes
0x0040bbf0    112  --------------- | Selecting %d out-of-bounds brushes
0x0040cbd0    917  Creating BSP. | _Vis.lek | Making VisBSP leak file: %s | Removing unseen geometry. | Recreating BSP. | Finding leaves. | -  %s (%d polies)
0x0040d2d0    327  66 | dtx_Create | 69 | 67
0x0040d417    459  66 | dtx_Create
0x0040eb50     16  DUMMY
0x0040f7c0    224  Default
0x0040f8f0    230  Default
0x00411690    510  MAIN_WORLD
0x00411920    595  AIVolume | MusicVolume | Pos | Terrain | ERROR: world model without a Name property found.
0x00411cb0    579  Solid | Nonexistant | Invisible | Translucent | SkyPortal | FullyBright | FlatShade | GouraudShade | LightMap | Subdivide | HullMaker | AlwaysLightMap | DirectionalLight | Portal | SkyPan | Additive | TimeOfDay | TerrainOccluder | VisBlocker | NotAStep | NoWallWalk | BlockLight
0x00411f30     34  DetailLevel
0x00412020    124  %f %f %f | true | false
0x004120a0     65  %f
0x004120f0    100  %f %f %f
0x00413270    367  ** Couldn't find any textures. | ** (Is project path set?) | Imported %d texture flags
0x004134e0    667  center | dirlight
0x00413780    564  %s\\%s | rb | LIGHTDEFS
0x00414790    800  Creating leak file %s | ** Couldn't find an inside leaf for leak file. | OutsideDef | ** Couldn't find an outside leaf. | ** (Or the world is sealed correctly). | ** Couldn't find a path from inside to outside. | wt | %f %f %f %f %f %f\n | ** Unable to open leak file %s.
0x00415cb0    894  Ambient light (%.1f, %.1f, %.1f)
0x004169b0    132  LightAnim_BASE
0x00416a40    182  Warning: couldn't load object.lto for light animations
0x00418b70    809  Can't create any threads to process lighting! | Warning: could only create %d threads for lighting.
0x00418ea0    222  WARNING: Invalid poly lightmap size, clamping.
0x00419e20   2639  ** Unable to get texture lights. | ** (Is project path set?) | Created %d texture lights.
0x0041a870   1677  Light | DirLight | StaticSunLight | InnerColor | OuterColor | Rotation | BrightScale | Bias | LightColor | Pos | ClipLight | FOV | LightRadius | Name
0x0041b240    134  BlockLight
0x0041cec0     98  %s\\object.lto
0x0041d3a0    337  LAnim - %s (%d frames) |       - %d bytes |       - discarding (nothing lit)
0x0041d790    382  LightGroup
0x0041d910   1287  Name | Object | Position | Color | Radius | ModelLight | Pos | LightColor | InnerColor | LightRadius | LightObjects
0x0041dfa0    126  Name
0x004207b0    267  basic_string
0x00421cb0     96  basic_string
0x00422310     59  basic_string
0x00422ad0     96  basic_string
0x00422e40     92  basic_string
0x00423aa0    259  basic_string
0x00423c10    251  basic_string
0x00424d90    177  PNB
0x00424e70    126  PNB
0x00425940    296  AmbientLight
0x00425a70     79  LightTableRes
0x00425ac0     79  PBlockSize
0x00425d20   3744  %d bytes for physics, %d blocks (%d average nodes per block, %d max)
0x0042c020   1945  NoSnap | ** Found a ZERO THICKNESS BRUSH (%d) | ** Unable to generate a plane (%d) | ** Found an out-of-bounds brush
0x0042ce40     89  GameStartPoint | StartPoint | Light | DirLight
0x0042f900   1419  PreProcessor | No world file specified. | .ed | %s.dat | skipdialog | 2.0 | Lithtech | Monolith Productions | Priority
0x00430140    204  Priority
0x00430220    117  %s\r\n
0x004302a0    144  Approximation
0x004304f0     22  Full Optimization
0x00430510     96  PreProcessor
0x00430640    123  Default
0x004306e0     82  textures\\default.dtx
0x00433000   1030  AIVolume | MusicVolume
0x0043e7e0   1303  LeakFile | PortalFile | MaxBounds | RaySelect | SelectProblemBrushes | SelectOutOfBoundsBrushes | SelectHullProblemBrushes | RemoveProblemObjects | TextureFlags | ProjectDir | NoPortals | bsp | aivolumes | replacetextures | numthreads | light | nocliplight | optimize | fast | LightAnims | onlylightverts | Lambertian | maxdetail | splitweight | logfile | ignorehidden | FixTJunc | NoSnapNoCSG | NoBalanceBSPs
0x0043ed20     86  %s\r\n
0x0043edc0    222  %s.ed | wb | Can't open ED file %s for writing.
0x0043eea0    365  Num detail nodes: %d
0x0043f020    232  Error making hulls.
0x0043f170    892  ERROR: No visibility BSP! | Error making PVS.
0x0043f5f0    463  --------------- | SelectHullProblemBrushes: Selected %d brushes. | Number of (unseen) polies removed: %d
0x0043fa90   1247  invisible.dtx | Effect | EffectParam | Name
0x0043ff70     97  MaxLMSize
0x0043ffe0     32  LMGridSize
0x00440040     37  Merging %d brushes.
0x00440082     61  --------------- | Found an invalid (unprocessable) brush!
0x004400c0    350  Creating physics BSP | PhysicsBSP | Creating visibility BSP | VisBSP
0x004403b0     80  Must have at least one brush!
0x00440400    713  --------------- | RaySelect: Did %d ray selections | WorldTree nodes: %d | WorldTree depth: %d | WorldTree root size: %.2f
0x004408d0     36  %d 
0x00440900   1558  Processing %s.ed | Date: %s, Time: %s | This machine has %d processor%s. | Using %d threads. | Max detail level: %d. | Getting objects from ED file. | --------------- | Removing problem objects | Loading DAT file. | Replacing textures... | Replaced %d textures | ------------------- | PortalFile - saving %s | Saving DAT file. | Max Lightmap Size: %.2f | Lightmap Grid Size: %.2f | Lighting world | Making PVS.
0x00440f3c    197  Ran out of memory! | Internal error!  (Old file format?)
0x00441010    128  %s_%d.log | rt | wt
0x004410c0    681  %s.ed | rb | * Object '%s' (class '%s') has an invalid position (%.2f %.2f %.2f). | Invalid ED file version (%d). | The ED file is corrupt (error code %d). | Can't open ED file: %s.
0x00441470    291  rb | Error loading %s. | DAT file %s is an invalid version. | DAT file %s is corrupt.
0x004415a0    692  .tmp | wb | Error opening output file.  Try again? | rb | ERROR IN CPreWorld::SaveFile! | Could not remove "%s", %s. | %s\nCould not rename "%s" to "%s", %s.\nTry again?\n(If you answer no, | No. | Yes. | %s  %s
0x00441860    492  Done in %.2f minutes | Number of input polies: %d | Number of input vertices: %d | Number of output polies: %d | Number of output vertices: %d | Tree depth: %d | Memory for vis lists: %d | Number of (unseen) polies removed: %d | Number of leaves: %d | Number of visible leaves: %d | Lightmap data size: %d | Lightmap compression ratio: %.2f | Number of objects: %d | Light table size (from LightTableRes): %dM %dk
0x00441a50    579  wt | File Version: %d\n\n | Leaves: %d\n | \n\nLeaf #%lu\n\n |   Polies: %d\n |     Connection: %lu\n |     Verts: %d\n |       %f %f %f\n |     Visibility: \n |     Visibility:  |     Center: %f %f %f\n
0x004431e0    116  %d
0x00443980    473  %d
0x00443b60    253  %d
0x00443c60    502  -replacetextures  | -textureflags  | -bsp  | -ignorehidden  | -FixTJunc  | -light  | -nocliplight  | -lightanims  | -onlylightverts  | -Lambertian  | -optimize  | -fast  | -aivolumes  | -logfile  | -projectdir  | -splitweight %f 
0x00443e60    594  rt | processor.opt
0x00444330    148  %d
0x004443d0    116  %d
0x00444470    346  wt | processor.opt | %s\n
0x004448b0     51  %f %f %f
0x00444910     32  PID
0x00444a10     33  %f %f %f
0x004450c0    614  RaySelecter | Rotation
0x004455d0    298  textures\\invisible.dtx
0x00446170    128  TerrainSubDivSize
0x0044626b    701  ****ERROR: can't get node path!
0x00446530   2634  ## Creating terrain '%s'. | %d surfaces merged. | %d terrain polies merged. | %d polies left. | ** Found terrain surface(s) with Lightmap TRUE.  Lightmaps will overfl | %d terrain sections (avg. %.1f polies/section) | %.1fx poly overlap (%d extra polies) | Generating %d terrain BSPs | CBspGen::GenerateBspTree returned FALSE
0x004475a0    921  *** Bad poly found! > 1 edge split! (%0.1f %0.1f %0.1f)
0x004479c0   3876  Fixing T-Junctions on %d polys... | *** WARNING *** WARNING *** WARNING *** | ** Polygon split processing terminated | ** due to full node tree.  Get rid of a | ** large number of T-Junctions and | ** try again. | ** Unable to complete T-Junction processing | ** due to invalid geometry.  Coordinates of | ** first point of current polygon: | **    %0.4f,%0.4f,%0.4f | Added %d polys for T-Junctions | Added %d verts for T-Junctions | No T-Junctions found
0x004498f0    226  InsideDef
0x0044acd0    251  FastApproxArea | Marked %d out of %d leaves as FastApprox
0x0044aff0    637  Zone | Can't find world model for zone '%s'
0x0044df60     72  CreateNodes, depth: %d, nNodes: %d
0x0044fcc0    234  1.0.4
0x004501d0    245  1.0.4
0x004508e0     25  P\tE
0x00450900     79  P\tE
0x00450b00    492  1.0.4
0x00451c30    270  1.0.4
0x00451d60   1040  unknown compression method | invalid window size | incorrect header check | incorrect data check | need dictionary
0x004522d0    114  DdH
0x00454340   3368  invalid block type | invalid stored block lengths | too many length or distance symbols | invalid bit length repeat
0x004550b0     85  oversubscribed dynamic bit lengths tree | incomplete dynamic bit lengths tree
0x004555d0    232  oversubscribed literal/length tree | incomplete literal/length tree
0x004558a0   1936  invalid literal/length code | invalid distance code
0x00456190    834  invalid distance code | invalid literal/length code
0x004564e0    284  ware | Software
```

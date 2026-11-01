# Microsoft Developer Studio Project File - Name="DEdit" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=DEdit - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "DEdit.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "DEdit.mak" CFG="DEdit - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "DEdit - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE 

# talon: DEdit.exe, the world editor of the AvP2 Editing Tools Update 1
# (and 2: the same file), built 2001-12-06. Written for this project: an
# MFC MDI application linked with MFC and the C runtime statically
# (nafxcw.lib's build 8569 objects, libcmt.lib and the old iostreams,
# libcimt.lib), its objects compiled by VC6 SP5 with the Processor Pack
# (cl 12.00.9044; build.mk's CL_DEdit), /O1 (unaligned functions, the
# __EH_prolog frames), /Gf (string literals pooled, in .data), no /GR,
# STLport (the LT2 copy, first in the includes: the image's
# std::length_error is _STL's), DirectDraw and Direct3D through the DirectX
# 8.1 SDK's headers and dxguid.lib (DX6's interfaces: IDirectDraw4,
# IDirect3D3); the LT2 libraries StdLith and RegMgr, and the 2001 ButeMgr,
# DibMgr, GenRegMgr, the LTA library, zlib and S3's s3tc.lib, all but
# s3tc.lib compiled with 9044 (DEdit.dsw). Its sources are Build 69's DEdit
# and shared engine, world and model code, being brought back to 2001
# (progress/DEdit_map.md).
#
# talon: the sources are listed in the shipped image's module order
# (progress/DEdit_map.md), which the link keeps: the Project Bar group
# first, then the rest of the project's sources, the shared code among
# them, in file name order, then MRCEXT and the trackers. Build 69's
# later code that the rest still calls comes last, in a group of its own.

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "DEdit - Win32 Release"

# PROP BASE Use_MFC 1
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 1
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\built\Release\tools\DEdit"
# PROP Intermediate_Dir "..\..\built\Release\tools\DEdit"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O1 /Gf /Op /I "..\..\lithshared\stl" /I "." /I "..\shared\model" /I "mrcext" /I "mrcext\vc_headers" /I "..\..\sdk\inc" /I "..\PreProcessor" /I "options" /I "..\shared\world" /I "misc" /I "dlgs" /I "controls" /I "frames" /I "project" /I "..\shared\engine" /I "regionedit" /I "draw" /I "docs" /I "views" /I "trackers" /I "modelview" /I "fileformats" /I "keyaggregate" /I "levelerror" /I "lightmap" /I "..\..\lithshared\stdlith" /I "..\..\lithshared\stdlith\sys\win" /I "..\..\lithshared\regmgr" /I "..\..\lithshared\lith" /I "..\..\..\Libs\ButeMgr" /I "..\..\lithshared\incs" /I "..\..\lithshared\ltamgr" /I "t:\dxsdk-8.1\include" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "WINVER=0x500" /D "REZMGRFULL" /D "DIRECTEDITOR_BUILD" /D "STDLITH_MFC" /D "BDEFS_MFC" /D "PRE_FLOAT" /D "PREPROCESSOR_BUILD" /D "NO_PRAGMA_LIBS" /D "LT15_COMPAT" /FD /c
# ADD CPP /nologo /MT /W3 /GX /O1 /Gf /Op /I "..\..\lithshared\stl" /I "." /I "..\shared\model" /I "mrcext" /I "mrcext\vc_headers" /I "..\..\sdk\inc" /I "..\PreProcessor" /I "options" /I "..\shared\world" /I "misc" /I "dlgs" /I "controls" /I "frames" /I "project" /I "..\shared\engine" /I "regionedit" /I "draw" /I "docs" /I "views" /I "trackers" /I "modelview" /I "fileformats" /I "keyaggregate" /I "levelerror" /I "lightmap" /I "..\..\lithshared\stdlith" /I "..\..\lithshared\stdlith\sys\win" /I "..\..\lithshared\regmgr" /I "..\..\lithshared\lith" /I "..\..\..\Libs\ButeMgr" /I "..\..\lithshared\incs" /I "..\..\lithshared\ltamgr" /I "t:\dxsdk-8.1\include" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "WINVER=0x500" /D "REZMGRFULL" /D "DIRECTEDITOR_BUILD" /D "STDLITH_MFC" /D "BDEFS_MFC" /D "PRE_FLOAT" /D "PREPROCESSOR_BUILD" /D "NO_PRAGMA_LIBS" /D "LT15_COMPAT" /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 ddraw.lib dxguid.lib winmm.lib /nologo /subsystem:windows /machine:I386 /out:"..\..\built\Release\tools\DEdit\DEdit.exe" /libpath:"t:\dxsdk-8.1\lib" /nodefaultlib:"\proj\libs\release\StdLith.lib" /nodefaultlib:"\proj\libs\release\StdLith_alloc.lib" /nodefaultlib:"\proj\libs\release\StdLithMFC.lib" /nodefaultlib:"\proj\libs\release\StdLithMFC_alloc.lib" /nodefaultlib:"\Proj\Libs\Release\lith.lib" /nodefaultlib:"\proj\libs\release\regmgr.lib" /nodefaultlib:"\proj\libs\release\GenRegMgr.lib" /nodefaultlib:"\proj\libs\release\dibmgr.lib" /nodefaultlib:"\Proj\Libs\Release\ButeMgr.lib" /nodefaultlib:"\Proj\Libs\Release\CryptMgr.lib" /nodefaultlib:"\proj\libs\release\zlib.lib"
# ADD LINK32 ddraw.lib dxguid.lib winmm.lib /nologo /subsystem:windows /machine:I386 /out:"..\..\built\Release\tools\DEdit\DEdit.exe" /libpath:"t:\dxsdk-8.1\lib" /nodefaultlib:"\proj\libs\release\StdLith.lib" /nodefaultlib:"\proj\libs\release\StdLith_alloc.lib" /nodefaultlib:"\proj\libs\release\StdLithMFC.lib" /nodefaultlib:"\proj\libs\release\StdLithMFC_alloc.lib" /nodefaultlib:"\Proj\Libs\Release\lith.lib" /nodefaultlib:"\proj\libs\release\regmgr.lib" /nodefaultlib:"\proj\libs\release\GenRegMgr.lib" /nodefaultlib:"\proj\libs\release\dibmgr.lib" /nodefaultlib:"\Proj\Libs\Release\ButeMgr.lib" /nodefaultlib:"\Proj\Libs\Release\CryptMgr.lib" /nodefaultlib:"\proj\libs\release\zlib.lib"

!ENDIF 

# Begin Target

# Name "DEdit - Win32 Release"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Group "Options Pages"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\OptionsSheet.cpp
# End Source File
# Begin Source File

SOURCE=.\UndoPage.cpp
# End Source File
# End Group
# Begin Group "Project Bar"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\ModelDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\NodeView.cpp
# End Source File
# Begin Source File

SOURCE=.\Controls\ProjectBar.cpp
# End Source File
# Begin Source File

SOURCE=.\Dlgs\PropertiesDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\SoundDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\SpriteDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\TextureDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\WorldsDlg.cpp
# End Source File
# End Group
# Begin Source File

SOURCE=.\RegionEdit\AdvancedSelect.cpp
# End Source File
# Begin Source File

SOURCE=.\Dlgs\AdvancedSelectDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\AllTextureDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\Dlgs\AlphaFromColorDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\Dlgs\AutoTextureOptionsDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\World\BasePoly.cpp
# End Source File
# Begin Source File

SOURCE=.\baserezdlg.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\engine\bdefs.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\engine\bindmgr.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\engine\classbind.cpp
# End Source File
# Begin Source File

SOURCE=.\Dlgs\ClassDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\ClassList.cpp
# End Source File
# Begin Source File

SOURCE=.\Dlgs\ClassListDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\Controls\ClassTree.cpp
# End Source File
# Begin Source File

SOURCE=.\FileFormats\CObjInterface.cpp
# End Source File
# Begin Source File

SOURCE=.\ColorPicker.cpp
# End Source File
# Begin Source File

SOURCE=.\Dlgs\ColorSelectDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\ColorWell.cpp
# End Source File
# Begin Source File

SOURCE=.\controls\ColumnCtrl.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\engine\concommand.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\engine\conparse.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\engine\converge.cpp
# End Source File
# Begin Source File

SOURCE=.\CylinderPrimDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\draw\d3d_texturemgr.cpp
# End Source File
# Begin Source File

SOURCE=.\Project\d_filemgr.cpp
# End Source File
# Begin Source File

SOURCE=.\Dlgs\DebugDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\DEdit.cpp
# End Source File
# Begin Source File

SOURCE=.\dedit_concommand.cpp
# End Source File
# Begin Source File

SOURCE=.\Options\DEditOptions.cpp
# End Source File
# Begin Source File

SOURCE=.\Frames\DEDockFrameWnd.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\engine\dhashtable.cpp
# End Source File
# Begin Source File

SOURCE=.\draw\draw_d3d.cpp
# End Source File
# Begin Source File

SOURCE=.\draw\DrawBase.cpp
# End Source File
# Begin Source File

SOURCE=.\dtxmgr.cpp
# End Source File
# Begin Source File

SOURCE=.\RegionEdit\edit_actions.cpp
# End Source File
# Begin Source File

SOURCE=.\World\EditBrush.cpp
# End Source File
# Begin Source File

SOURCE=.\RegionEdit\EditClipboard.cpp
# End Source File
# Begin Source File

SOURCE=.\RegionEdit\EditGrid.cpp
# End Source File
# Begin Source File

SOURCE=.\Misc\EditHelpers.cpp
# End Source File
# Begin Source File

SOURCE=.\World\EditObjects.cpp
# End Source File
# Begin Source File

SOURCE=.\Misc\EditorTransfer.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\world\EditPlane.cpp
# End Source File
# Begin Source File

SOURCE=.\World\EditPoly.cpp
# End Source File
# Begin Source File

SOURCE=.\Project\EditProjectMgr.cpp
# End Source File
# Begin Source File

SOURCE=.\World\EditRegion.cpp
# End Source File
# Begin Source File

SOURCE=.\draw\EditSoftRender.cpp
# End Source File
# Begin Source File

SOURCE=.\ExportObjFile.cpp
# End Source File
# Begin Source File

SOURCE=.\FilePalette.cpp
# End Source File
# Begin Source File

SOURCE=.\Misc\FileUtils.cpp
# End Source File
# Begin Source File

SOURCE=.\Frames\flattoolbar.cpp
# End Source File
# Begin Source File

SOURCE=.\Controls\FrameList.cpp
# End Source File
# Begin Source File

SOURCE=.\dlgs\GenerateUniqueNamesDlg.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\engine\genericprop_setup.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\engine\genltstream.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\engine\geomroutines.cpp
# End Source File
# Begin Source File

SOURCE=.\dlgs\ImportObjectDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\ImportObjFileFormat.cpp
# End Source File
# Begin Source File

SOURCE=.\KeyMapDlg.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\engine\light_table.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\engine\lightmap_compress.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\engine\lightmap_planes.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\engine\load_pcx.cpp
# End Source File
# Begin Source File

SOURCE=.\LoadLTADialog.cpp
# End Source File
# Begin Source File

SOURCE=.\LoadModelDims.cpp
# End Source File
# Begin Source File

SOURCE=.\LTEulerAngles.cpp
# End Source File
# Begin Source File

SOURCE=..\..\sdk\inc\ltquatbase.cpp
# End Source File
# Begin Source File

SOURCE=.\Frames\MainFrm.cpp
# End Source File
# Begin Source File

SOURCE=.\MakeRezFile.cpp
# End Source File
# Begin Source File

SOURCE=.\MapTextureCoordsDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\mlistctrl.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\model\model.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\model\model_load.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\model\model_ops.cpp
# talon: it includes <windows.h> before bdefs.h, as ModelEdit's
# compile has it: without MFC
# SUBTRACT CPP /D "STDLITH_MFC" /D "BDEFS_MFC"
# End Source File
# Begin Source File

SOURCE=..\shared\model\modelallocations.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\model\modelcallbacks.cpp
# End Source File
# Begin Source File

SOURCE=.\mtreectl.cpp
# End Source File
# Begin Source File

SOURCE=.\MultiLineStringDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\dlgs\NameChangeReportDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\draw\Navigator.cpp
# End Source File
# Begin Source File

SOURCE=.\dlgs\navigatordlg.cpp
# End Source File
# Begin Source File

SOURCE=.\dlgs\NavigatorStoreDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\Dlgs\NewProjectDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\World\node_ops.cpp
# End Source File
# Begin Source File

SOURCE=.\NodeViewTreeCtrl.cpp
# End Source File
# Begin Source File

SOURCE=.\dlgs\ObjectBrowserDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\ObjectImporter.cpp
# End Source File
# Begin Source File

SOURCE=.\Options\OptionsAdvancedSelect.cpp
# End Source File
# Begin Source File

SOURCE=.\Options\OptionsBase.cpp
# End Source File
# Begin Source File

SOURCE=.\Options\OptionsClassDialog.cpp
# End Source File
# Begin Source File

SOURCE=.\Options\OptionsClipboard.cpp
# End Source File
# Begin Source File

SOURCE=.\Options\OptionsDisplay.cpp
# End Source File
# Begin Source File

SOURCE=.\Options\OptionsGenerateUniqueNames.cpp
# End Source File
# Begin Source File

SOURCE=.\Options\OptionsObjectBrowser.cpp
# End Source File
# Begin Source File

SOURCE=.\Options\OptionsPrimitives.cpp
# End Source File
# Begin Source File

SOURCE=.\Options\OptionsRun.cpp
# End Source File
# Begin Source File

SOURCE=.\Options\OptionsWarnings.cpp
# End Source File
# Begin Source File

SOURCE=.\Options\OptionsWindows.cpp
# End Source File
# Begin Source File

SOURCE=.\PaletteEdit.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\engine\pixelformat.cpp
# End Source File
# Begin Source File

SOURCE=.\dlgs\PopupNoteDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\PrefabDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\Dlgs\PreferencesDlg.cpp
# End Source File
# Begin Source File

SOURCE=..\PreProcessor\pregeometry.cpp
# End Source File
# Begin Source File

SOURCE=..\PreProcessor\PreLeaf.cpp
# End Source File
# Begin Source File

SOURCE=..\PreProcessor\PreLightMap.cpp
# End Source File
# Begin Source File

SOURCE=..\PreProcessor\PrePoly.cpp
# End Source File
# Begin Source File

SOURCE=..\PreProcessor\PreSurface.cpp
# End Source File
# Begin Source File

SOURCE=..\PreProcessor\PreWorld.cpp
# End Source File
# Begin Source File

SOURCE=.\Controls\ProjectTabControlBar.cpp
# End Source File
# Begin Source File

SOURCE=.\controls\ProjectTabCtrl.cpp
# End Source File
# Begin Source File

SOURCE=.\controls\PropEditString.cpp
# End Source File
# Begin Source File

SOURCE=.\Controls\PropEditStringList.cpp
# End Source File
# Begin Source File

SOURCE=.\Docs\PropertyHelpers.cpp
# End Source File
# Begin Source File

SOURCE=.\World\PropList.cpp
# End Source File
# Begin Source File

SOURCE=.\dlgs\PropPageDisplay.cpp
# End Source File
# Begin Source File

SOURCE=.\dlgs\PropPageOptionsClipboard.cpp
# End Source File
# Begin Source File

SOURCE=.\dlgs\PropPageOptionsD3D.cpp
# End Source File
# Begin Source File

SOURCE=.\dlgs\PropPageOptionsRun.cpp
# End Source File
# Begin Source File

SOURCE=.\RegionEdit\Refs.cpp
# End Source File
# Begin Source File

SOURCE=.\Docs\RegionDoc.cpp
# End Source File
# Begin Source File

SOURCE=.\Frames\RegionFrame.cpp
# End Source File
# Begin Source File

SOURCE=.\Views\RegionView.cpp
# End Source File
# Begin Source File

SOURCE=.\Controls\ResizeBar.cpp
# End Source File
# Begin Source File

SOURCE=.\ResizeDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\ResNewDir.cpp
# End Source File
# Begin Source File

SOURCE=.\ResourceMgr.cpp
# End Source File
# Begin Source File

SOURCE=.\RezBuildDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\RotateSelection.cpp
# End Source File
# Begin Source File

SOURCE=.\Dlgs\RotationEdit.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\world\s3tc_compress.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\engine\simple_dalloc.cpp
# End Source File
# Begin Source File

SOURCE=.\dlgs\SpherePrimitiveDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\Misc\Splash.cpp
# End Source File
# Begin Source File

SOURCE=.\Dlgs\SpriteEditDlg.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\engine\streamsim.cpp
# End Source File
# Begin Source File

SOURCE=.\Dlgs\StringDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\Misc\Texture.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\world\TexturedPlane.cpp
# End Source File
# Begin Source File

SOURCE=.\TextureProp.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\model\transformmaker.cpp
# talon: it includes <windows.h> before bdefs.h, as ModelEdit's
# compile has it: without MFC
# SUBTRACT CPP /D "STDLITH_MFC" /D "BDEFS_MFC"
# End Source File
# Begin Source File

SOURCE=.\UIEvent.cpp
# End Source File
# Begin Source File

SOURCE=.\RegionEdit\undo_mgr.cpp
# End Source File
# Begin Source File

SOURCE=.\draw\UsefulDib.cpp
# End Source File
# Begin Source File

SOURCE=.\VectorButton.cpp
# End Source File
# Begin Source File

SOURCE=.\VectorEdit.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\engine\version_info.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\engine\version_resource.cpp
# End Source File
# Begin Source File

SOURCE=.\draw\ViewDef.cpp
# End Source File
# Begin Source File

SOURCE=.\RegionEdit\ViewFunctions.cpp
# End Source File
# Begin Source File

SOURCE=.\RegionEdit\viewrender.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\engine\world_tree.cpp
# End Source File
# Begin Source File

SOURCE=.\Dlgs\WorldInfoDlg.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\world\WorldNode.cpp
# End Source File
# End Group
# Begin Group "mrcext"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\MRCEXT\DRAGDOCK.CPP
# End Source File
# Begin Source File

SOURCE=.\MRCEXT\FRAMEWND.CPP
# End Source File
# Begin Source File

SOURCE=.\MRCEXT\MDIFLOAT.CPP
# End Source File
# Begin Source File

SOURCE=.\MRCEXT\SIZECONT.CPP
# End Source File
# Begin Source File

SOURCE=.\MRCEXT\SIZEDLG.CPP
# End Source File
# Begin Source File

SOURCE=.\MRCEXT\SIZEDOCK.CPP
# End Source File
# Begin Source File

SOURCE=.\MRCEXT\SZTOOLBA.CPP
# End Source File
# End Group
# Begin Group "Trackers"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Trackers\RVCommand.cpp
# End Source File
# Begin Source File

SOURCE=.\Trackers\RVTracker.cpp
# End Source File
# Begin Source File

SOURCE=.\Trackers\RVTrackerBrushSize.cpp
# End Source File
# Begin Source File

SOURCE=.\Trackers\RVTrackerCurveEdit.cpp
# End Source File
# Begin Source File

SOURCE=.\Trackers\RVTrackerDrawPoly.cpp
# End Source File
# Begin Source File

SOURCE=.\Trackers\RVTrackerFarDist.cpp
# End Source File
# Begin Source File

SOURCE=.\Trackers\RVTrackerFocus.cpp
# End Source File
# Begin Source File

SOURCE=.\Trackers\RVTrackerGridSize.cpp
# End Source File
# Begin Source File

SOURCE=.\Trackers\RVTrackerMarkerMove.cpp
# End Source File
# Begin Source File

SOURCE=.\Trackers\RVTrackerNavMove.cpp
# End Source File
# Begin Source File

SOURCE=.\Trackers\RVTrackerNavRotate.cpp
# End Source File
# Begin Source File

SOURCE=.\Trackers\RVTrackerNodeMove.cpp
# End Source File
# Begin Source File

SOURCE=.\Trackers\RVTrackerNodeRotate.cpp
# End Source File
# Begin Source File

SOURCE=.\Trackers\RVTrackerTag.cpp
# End Source File
# Begin Source File

SOURCE=.\Trackers\RVTrackerTextMove.cpp
# End Source File
# Begin Source File

SOURCE=.\Trackers\RVTrackerTextWrap.cpp
# End Source File
# Begin Source File

SOURCE=.\Trackers\RVTrackerVertMove.cpp
# End Source File
# Begin Source File

SOURCE=.\Trackers\RVTrackerVertScale.cpp
# End Source File
# Begin Source File

SOURCE=.\Trackers\RVTrackerVertSnap.cpp
# End Source File
# Begin Source File

SOURCE=.\Trackers\TrackerMgr.cpp
# End Source File
# End Group
# Begin Group "libs"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\shared\world\s3tc.lib
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\DEdit.rc
# End Source File
# End Group
# End Target
# End Project

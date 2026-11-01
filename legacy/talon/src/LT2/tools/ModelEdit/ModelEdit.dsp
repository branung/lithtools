# Microsoft Developer Studio Project File - Name="ModelEdit" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=ModelEdit - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "ModelEdit.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "ModelEdit.mak" CFG="ModelEdit - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "ModelEdit - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE 

# talon: ModelEdit.exe, the model editor of the AvP2 Editing Tools Update 1
# (and 2: the same file), "ModelEdit for Talon", built 2001-12-19. Written
# for this project: an MFC dialog application linked with MFC and the C
# runtime statically (nafxcw.lib's build 8569 objects, libcmt.lib, VC6's own
# C++ library libcpmt.lib and the old iostreams libcimt.lib), its objects
# compiled by VC6 SP5 with the Processor Pack (cl 12.00.9044; build.mk's
# CL_ModelEdit), /O1 (unaligned functions), no /GF (literals in .data, one
# copy per object), no /GR; the LT2 libraries StdLith and RegMgr. Its
# sources are Build 69's ModelEdit and shared model code, being brought back
# to 2001 (progress/ModelEdit_map.md).
#
# talon: the sources are listed in the shipped image's module order
# (progress/ModelEdit_map.md), which the link keeps: the project's own
# folder, in name order (with animtracker, the LTA importer, ltquatbase and
# smmempnproc among them), then the shared engine and model code, in name
# order. The 2001 LTA parse-node reader (cstream, ltaBuildTree, ltaFindPN,
# ltalex, ltaParseNode) and LVEditList have no surviving source: written
# from the image, their file names are ours.
#
# talon: /I "stlpatch": Dinkumware's patched <xstring> (25 May 1998, no
# reference counting; Jupiter Build 69 ships it in WONAPI's
# VC_STL_Header_Patches), which the image's std::string code is compiled
# from (_Tidy 0x409e80). Only that header: its std::map is cl 9044's own.

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "ModelEdit - Win32 Release"

# PROP BASE Use_MFC 1
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 1
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\built\Release\tools\ModelEdit"
# PROP Intermediate_Dir "..\..\built\Release\tools\ModelEdit"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O1 /I "." /I "stlpatch" /I "..\shared\engine" /I "..\shared\model" /I "..\..\sdk\inc" /I "..\..\lithshared\stdlith" /I "..\..\lithshared\regmgr" /I "..\..\lithshared\lith" /I "..\..\lithshared\incs" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "WINVER=0x500" /D "MODEL_STANDALONE" /D "BDEFS_SIM" /D "LT15_COMPAT" /D "NO_PRAGMA_LIBS" /D "BDEFS_WINDOWS_H" /FD /c
# ADD CPP /nologo /MT /W3 /GX /O1 /I "." /I "stlpatch" /I "..\shared\engine" /I "..\shared\model" /I "..\..\sdk\inc" /I "..\..\lithshared\stdlith" /I "..\..\lithshared\regmgr" /I "..\..\lithshared\lith" /I "..\..\lithshared\incs" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "WINVER=0x500" /D "MODEL_STANDALONE" /D "BDEFS_SIM" /D "LT15_COMPAT" /D "NO_PRAGMA_LIBS" /D "BDEFS_WINDOWS_H" /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 winmm.lib opengl32.lib glu32.lib /nologo /subsystem:windows /machine:I386 /out:"..\..\built\Release\tools\ModelEdit\ModelEdit.exe" /nodefaultlib:"\Proj\Libs\Release\lith.lib" /nodefaultlib:"\proj\libs\release\regmgr.lib" /nodefaultlib:"\proj\libs\release\StdLith.lib" /nodefaultlib:"\proj\libs\release\StdLith_alloc.lib"
# ADD LINK32 winmm.lib opengl32.lib glu32.lib /nologo /subsystem:windows /machine:I386 /out:"..\..\built\Release\tools\ModelEdit\ModelEdit.exe" /nodefaultlib:"\Proj\Libs\Release\lith.lib" /nodefaultlib:"\proj\libs\release\regmgr.lib" /nodefaultlib:"\proj\libs\release\StdLith.lib" /nodefaultlib:"\proj\libs\release\StdLith_alloc.lib"

!ENDIF 

# Begin Target

# Name "ModelEdit - Win32 Release"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\AboutDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\AddChildModelDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\AddSocketDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\AnimCleanupDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\AnimFramerateDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\AnimMergeDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\animnumberdlg.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\model\animtracker.cpp
# End Source File
# Begin Source File

SOURCE=.\CommandStringDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\ContinuousDlg.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\model\cstream.cpp
# End Source File
# Begin Source File

SOURCE=.\DimensionsDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\DragList.cpp
# End Source File
# Begin Source File

SOURCE=.\LTEulerAngles.cpp
# End Source File
# Begin Source File

SOURCE=.\GenLodDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\gl_modelrender.cpp
# End Source File
# Begin Source File

SOURCE=.\ImportAnimation.cpp
# End Source File
# Begin Source File

SOURCE=.\KeyframeStringDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\KeyframeTimeDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\KeyframeWnd.cpp
# End Source File
# Begin Source File

SOURCE=.\loadltadlg.cpp
# End Source File
# Begin Source File

SOURCE=.\LODEdit.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\model\ltaBuildTree.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\model\ltaFindPN.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\model\ltalex.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\model\ltaModel.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\model\ltaParseNode.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\model\ltaScene.cpp
# End Source File
# Begin Source File

SOURCE=..\..\sdk\inc\ltquatbase.cpp
# End Source File
# Begin Source File

SOURCE=.\LVEditList.cpp
# End Source File
# Begin Source File

SOURCE=.\ModelEdit.cpp
# End Source File
# Begin Source File

SOURCE=.\ModelEditDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\ModelInfoDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\MyEdit.cpp
# End Source File
# Begin Source File

SOURCE=.\newgenlod.cpp
# End Source File
# Begin Source File

SOURCE=.\PieceInfoDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\RenameDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\RenameNodeDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\RenderWnd.cpp
# End Source File
# Begin Source File

SOURCE=.\RotationDlg.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\model\smmempnproc.cpp
# End Source File
# Begin Source File

SOURCE=.\SocketEdit.cpp
# End Source File
# Begin Source File

SOURCE=.\SolidRectWnd.cpp
# End Source File
# Begin Source File

SOURCE=.\StdAfx.cpp
# End Source File
# Begin Source File

SOURCE=.\StringDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\TimeOffsetDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\TranslationDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\UVImportDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\WeightEditDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\WeightSetSelectDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\win_idle.cpp
# End Source File
# End Group
# Begin Group "Shared"

# PROP Default_Filter "cpp;c"
# Begin Source File

SOURCE=.\bdefs.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\engine\conparse.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\engine\counter.cpp
# End Source File
# Begin Source File

SOURCE=.\dtxmgr.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\engine\genltstream.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\engine\geomroutines.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\model\model.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\model\model_cleanup.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\model\model_load.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\model\model_ops.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\model\model_save.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\model\modelallocations.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\model\modelcallbacks.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\engine\pixelformat.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\engine\simple_dalloc.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\engine\streamsim.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\model\transformmaker.cpp
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\ModelEdit.rc
# End Source File
# End Group
# End Target
# End Project

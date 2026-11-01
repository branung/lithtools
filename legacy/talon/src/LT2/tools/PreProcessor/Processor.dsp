# Microsoft Developer Studio Project File - Name="Processor" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=Processor - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "Processor.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "Processor.mak" CFG="Processor - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "Processor - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE 

# talon: Processor.exe, the LithTech world processor of the AvP2 Editing
# Tools v1.0 (built 2001-10-03). Written for this project: an MFC
# application linked with MFC and the C runtime statically (the shipped
# image imports neither; its Rich header names nafxcw.lib's build 8569
# objects and libcmt.lib's), VC6 SP5's compiler (8966) and linker, the LT2
# libraries StdLith, zlib and RegMgr (lithshared.dsw). Its sources are the
# 2001 processor's, written from the image, and the shared engine and world
# code taken from Build 69 and brought back to 2001.
#
# talon: the sources are listed in the shipped image's module order
# (progress/Processor_avp2_map.md), which the link keeps: nearly the file
# names' order (Threads comes after the T-junctions; bdefs.cpp's
# g_ReturnErrString is the first thing in .data, 0x47e100). The libraries' #pragma comment(lib) names
# (\proj\libs\...) are not linked: the project links the LT2 ones.

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "Processor - Win32 Release"

# PROP BASE Use_MFC 1
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 1
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\built\Release\tools\Processor"
# PROP Intermediate_Dir "..\..\built\Release\tools\Processor"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W2 /GX /O2 /I "." /I "..\shared\engine" /I "..\shared\world" /I "win" /I "..\DEdit\draw" /I "..\DEdit\lightmap" /I "..\..\sdk\inc" /I "..\..\lithshared\stdlith" /I "..\..\lithshared\stdlith\sys\win" /I "..\..\lithshared\regmgr" /I "..\..\lithshared\lith" /I "..\..\lithshared\incs" /I "..\..\lithshared\stl" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "PRE_FLOAT" /D "PREPROCESSOR_BUILD" /D "BDEFS_MFC" /D "STDLITH_MFC" /FD /c
# ADD CPP /nologo /MT /W2 /GX /O2 /I "." /I "..\shared\engine" /I "..\shared\world" /I "win" /I "..\DEdit\draw" /I "..\DEdit\lightmap" /I "..\..\sdk\inc" /I "..\..\lithshared\stdlith" /I "..\..\lithshared\stdlith\sys\win" /I "..\..\lithshared\regmgr" /I "..\..\lithshared\lith" /I "..\..\lithshared\incs" /I "..\..\lithshared\stl" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "PRE_FLOAT" /D "PREPROCESSOR_BUILD" /D "BDEFS_MFC" /D "STDLITH_MFC" /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 winmm.lib /nologo /subsystem:windows /machine:I386 /out:"..\..\built\Release\tools\Processor\Processor.exe" /nodefaultlib:"\proj\libs\release\StdLithMFC.lib" /nodefaultlib:"\proj\libs\release\StdLith.lib" /nodefaultlib:"\proj\libs\release\zlib.lib" /nodefaultlib:"\Proj\Libs\Release\lith.lib" /nodefaultlib:"\proj\libs\release\regmgr.lib"
# ADD LINK32 winmm.lib /nologo /subsystem:windows /machine:I386 /out:"..\..\built\Release\tools\Processor\Processor.exe" /nodefaultlib:"\proj\libs\release\StdLithMFC.lib" /nodefaultlib:"\proj\libs\release\StdLith.lib" /nodefaultlib:"\proj\libs\release\zlib.lib" /nodefaultlib:"\Proj\Libs\Release\lith.lib" /nodefaultlib:"\proj\libs\release\regmgr.lib"

!ENDIF 

# Begin Target

# Name "Processor - Win32 Release"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\shared\world\BasePoly.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\engine\bdefs.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\engine\bindmgr.cpp
# End Source File
# Begin Source File

SOURCE=.\BrushToWorld.cpp
# End Source File
# Begin Source File

SOURCE=.\BspGen.cpp
# End Source File
# Begin Source File

SOURCE=.\BuildHulls.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\engine\classbind.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\engine\conparse.cpp
# End Source File
# Begin Source File

SOURCE=.\create_world_tree.cpp
# End Source File
# Begin Source File

SOURCE=.\MakeBsps.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\engine\dtxmgr.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\world\EditBrush.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\world\EditObjects.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\world\EditPoly.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\world\EditRegion.cpp
# End Source File
# Begin Source File

SOURCE=.\FindWorldModel.cpp
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

SOURCE=.\gettextureflags.cpp
# End Source File
# Begin Source File

SOURCE=.\gettextureinfo.cpp
# End Source File
# Begin Source File

SOURCE=.\LeafPortals.cpp
# End Source File
# Begin Source File

SOURCE=.\LeakFile.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\engine\light_table.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\engine\lightmap_compress.cpp
# End Source File
# Begin Source File

SOURCE=.\lightmap_planes.cpp
# End Source File
# Begin Source File

SOURCE=.\LightMapMaker.cpp
# End Source File
# Begin Source File

SOURCE=..\..\sdk\inc\ltquatbase.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\engine\mathlt_impl.cpp
# End Source File
# Begin Source File

SOURCE=..\DEdit\draw\Navigator.cpp
# End Source File
# Begin Source File

SOURCE=.\Node.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\world\node_ops.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\engine\parse_world_info.cpp
# End Source File
# Begin Source File

SOURCE=.\PhysicsBlocks.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\engine\pixelformat.cpp
# End Source File
# Begin Source File

SOURCE=.\PreBrush.cpp
# End Source File
# Begin Source File

SOURCE=.\pregeometry.cpp
# End Source File
# Begin Source File

SOURCE=.\PreLeaf.cpp
# End Source File
# Begin Source File

SOURCE=.\PreLightMap.cpp
# End Source File
# Begin Source File

SOURCE=.\PrePoly.cpp
# End Source File
# Begin Source File

SOURCE=.\ProcessorApp.cpp
# End Source File
# Begin Source File

SOURCE=.\PreSurface.cpp
# End Source File
# Begin Source File

SOURCE=.\PreWorld.cpp
# End Source File
# Begin Source File

SOURCE=.\Processing.cpp
# End Source File
# Begin Source File

SOURCE=.\ProcessorOptions.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\world\PropList.cpp
# End Source File
# Begin Source File

SOURCE=.\RaySelecter.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\engine\streamsim.cpp
# End Source File
# Begin Source File

SOURCE=.\Terrain.cpp
# End Source File
# Begin Source File

SOURCE=.\TJunctions.cpp
# End Source File
# Begin Source File

SOURCE=.\Win\Threads.cpp
# End Source File
# Begin Source File

SOURCE=.\VisLeaves.cpp
# End Source File
# Begin Source File

SOURCE=.\WorldApprox.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\engine\world_tree.cpp
# End Source File
# Begin Source File

SOURCE=..\shared\world\WorldNode.cpp
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\PreProcessor.rc
# End Source File
# Begin Source File

SOURCE=.\Res\PreProcessor.ico
# End Source File
# Begin Source File

SOURCE=.\resource.h
# End Source File
# End Group
# End Target
# End Project

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

# talon: Processor.exe, the LithTech world processor of NOLF Tools 001
# (built 2001-01-03; it writes NOLF's world version 66). Written for this
# project, from the AvP2 processor's (../PreProcessor/Processor.dsp, nine
# months later), brought back to January 2001. What built it
# (progress/Processor_nolf_map.md): its own objects are Intel C/C++ 4.0's
# (no @comp.id: the Rich header's 82 objects without one; .data1 holds their
# literals; the prologue and lea-nop padding are icl's), /O1, but PreWorld.cpp,
# which is VC6 SP3's (cl 12.00.8447) /O1 (0x442f08..0x44ce10: __EH_prolog,
# unaligned functions); MFC (nafxcw.lib), the C runtime and the LT2 libraries
# are 8447's, linked statically by VC6 SP3's linker. build.mk sets the
# compilers (CL_ProcessorNOLF, CL_ProcessorNOLF__PreWorld).
#
# talon: icl's /EHa (asynchronous EH, which PreWorld.cpp's cl doesn't get): a
# new-expression keeps `this` in a stack slot it never reads and stores the
# result twice more (CBoolProp::CreateSameKind 0x454b80), which /GX alone
# doesn't give.
#
# talon: the sources are listed in the shipped image's module order
# (progress/Processor_nolf_map.md), which the link keeps. The libraries'
# #pragma comment(lib) names (\proj\libs\...) are not linked: the project
# links the LT2 ones.

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
# PROP Output_Dir "..\..\built\Release\tools\ProcessorNOLF"
# PROP Intermediate_Dir "..\..\built\Release\tools\ProcessorNOLF"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W2 /GX /O1 /I "." /I "..\sharedNOLF\engine" /I "..\sharedNOLF\world" /I "win" /I "..\DEdit\draw" /I "..\DEdit\lightmap" /I "..\..\sdkNOLF\inc" /I "..\..\lithshared\stdlithNOLF" /I "..\..\lithshared\stdlith\sys\win" /I "..\..\lithshared\regmgr" /I "..\..\lithshared\lith" /I "..\..\lithshared\incs" /I "..\..\lithshared\stl" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "PRE_FLOAT" /D "PREPROCESSOR_BUILD" /D "BDEFS_MFC" /D "STDLITH_MFC" /FD /c
# ADD CPP /nologo /MT /W2 /GX /EHa /O1 /I "." /I "..\sharedNOLF\engine" /I "..\sharedNOLF\world" /I "win" /I "..\DEdit\draw" /I "..\DEdit\lightmap" /I "..\..\sdkNOLF\inc" /I "..\..\lithshared\stdlithNOLF" /I "..\..\lithshared\stdlith\sys\win" /I "..\..\lithshared\regmgr" /I "..\..\lithshared\lith" /I "..\..\lithshared\incs" /I "..\..\lithshared\stl" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "PRE_FLOAT" /D "PREPROCESSOR_BUILD" /D "BDEFS_MFC" /D "STDLITH_MFC" /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 winmm.lib /nologo /subsystem:windows /machine:I386 /out:"..\..\built\Release\tools\ProcessorNOLF\Processor.exe" /nodefaultlib:"\proj\libs\release\StdLithMFC.lib" /nodefaultlib:"\proj\libs\release\StdLith.lib" /nodefaultlib:"\proj\libs\release\zlib.lib" /nodefaultlib:"\Proj\Libs\Release\lith.lib" /nodefaultlib:"\proj\libs\release\regmgr.lib"
# ADD LINK32 winmm.lib /nologo /subsystem:windows /machine:I386 /out:"..\..\built\Release\tools\ProcessorNOLF\Processor.exe" /nodefaultlib:"\proj\libs\release\StdLithMFC.lib" /nodefaultlib:"\proj\libs\release\StdLith.lib" /nodefaultlib:"\proj\libs\release\zlib.lib" /nodefaultlib:"\Proj\Libs\Release\lith.lib" /nodefaultlib:"\proj\libs\release\regmgr.lib"

!ENDIF 

# Begin Target

# Name "Processor - Win32 Release"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\sharedNOLF\world\BasePoly.cpp
# End Source File
# Begin Source File

SOURCE=..\sharedNOLF\engine\bdefs.cpp
# End Source File
# Begin Source File

SOURCE=..\sharedNOLF\engine\bindmgr.cpp
# End Source File
# Begin Source File

SOURCE=.\BrushToWorld.cpp
# End Source File
# Begin Source File

SOURCE=.\BspGen.cpp
# End Source File
# Begin Source File

SOURCE=.\BuildHulls.cpp
# talon: /Ob0, a stand-in as streamsim.cpp's: more matches with it than without

!IF  "$(CFG)" == "Processor - Win32 Release"

# ADD CPP /Ob0

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\sharedNOLF\engine\classbind.cpp
# End Source File
# Begin Source File

SOURCE=..\sharedNOLF\engine\conparse.cpp
# End Source File
# Begin Source File

SOURCE=.\create_world_tree.cpp
# talon: /Ob0, a stand-in as streamsim.cpp's: more matches with it than without

!IF  "$(CFG)" == "Processor - Win32 Release"

# ADD CPP /Ob0

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\MakeBsps.cpp
# End Source File
# Begin Source File

SOURCE=..\sharedNOLF\engine\dtxmgr.cpp
# End Source File
# Begin Source File

SOURCE=..\sharedNOLF\world\EditBrush.cpp
# End Source File
# Begin Source File

SOURCE=..\sharedNOLF\world\EditObjects.cpp
# talon: /Ob0, a stand-in as streamsim.cpp's: more matches with it than without

!IF  "$(CFG)" == "Processor - Win32 Release"

# ADD CPP /Ob0

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\sharedNOLF\world\EditPoly.cpp
# End Source File
# Begin Source File

SOURCE=..\sharedNOLF\world\EditRegion.cpp
# End Source File
# Begin Source File

SOURCE=.\FindWorldModel.cpp
# End Source File
# Begin Source File

SOURCE=..\sharedNOLF\engine\genericprop_setup.cpp
# End Source File
# Begin Source File

SOURCE=..\sharedNOLF\engine\genltstream.cpp
# End Source File
# Begin Source File

SOURCE=..\sharedNOLF\engine\geomroutines.cpp
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

SOURCE=..\sharedNOLF\engine\light_table.cpp
# End Source File
# Begin Source File

SOURCE=..\sharedNOLF\engine\lightmap_compress.cpp
# End Source File
# Begin Source File

SOURCE=.\lightmap_planes.cpp
# End Source File
# Begin Source File

SOURCE=.\LightMapMaker.cpp
# End Source File
# Begin Source File

SOURCE=..\..\sdkNOLF\inc\ltquatbase.cpp
# End Source File
# Begin Source File

SOURCE=..\sharedNOLF\engine\mathlt_impl.cpp
# End Source File
# Begin Source File

SOURCE=..\DEdit\draw\Navigator.cpp
# End Source File
# Begin Source File

SOURCE=.\Node.cpp
# End Source File
# Begin Source File

SOURCE=..\sharedNOLF\world\node_ops.cpp
# End Source File
# Begin Source File

SOURCE=..\sharedNOLF\engine\parse_world_info.cpp
# End Source File
# Begin Source File

SOURCE=.\PhysicsBlocks.cpp
# End Source File
# Begin Source File

SOURCE=..\sharedNOLF\engine\pixelformat.cpp
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
# talon: /Ob0, a stand-in as streamsim.cpp's: more matches with it than without

!IF  "$(CFG)" == "Processor - Win32 Release"

# ADD CPP /Ob0

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\PreWorld.cpp

!IF  "$(CFG)" == "Processor - Win32 Release"

# SUBTRACT CPP /EHa

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\Processing.cpp
# End Source File
# Begin Source File

SOURCE=.\ProcessorOptions.cpp
# End Source File
# Begin Source File

SOURCE=..\sharedNOLF\world\PropList.cpp
# End Source File
# Begin Source File

SOURCE=.\RaySelecter.cpp
# End Source File
# Begin Source File

SOURCE=..\sharedNOLF\engine\streamsim.cpp
# talon: a stand-in. The shipped icl inlines nothing into a function with an
# EH frame (ILTStream's 13-byte constructor is called out of line, 0x456ab0)
# and inlines as usual elsewhere; ours (4.0, build 99100) inlines into both.
# /Ob0 here gives the EH-framed constructors, destructors and streamsim_*
# (14 rows) and loses four that inline (SSMemStream::SeekTo, ::Write, ::GetLen,
# SSBufStream::Write).

!IF  "$(CFG)" == "Processor - Win32 Release"

# ADD CPP /Ob0

!ENDIF 

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

SOURCE=..\sharedNOLF\engine\world_tree.cpp
# End Source File
# Begin Source File

SOURCE=..\sharedNOLF\world\WorldNode.cpp
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

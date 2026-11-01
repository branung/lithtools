# Microsoft Developer Studio Project File - Name="Talon_GenRegMgr" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=Talon_GenRegMgr - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "genregmgr60_Talon.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "genregmgr60_Talon.mak" CFG="Talon_GenRegMgr - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "Talon_GenRegMgr - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE 

# talon: GenRegMgr as the Talon DEdit (2001-12-06) links it: build 51's
# talon: source brought back to the LT2 header (incs/genregmgr.h: DWORDs), the
# talon: Release configuration's settings, compiled by VC6 SP5 with the
# talon: Processor Pack (cl 12.00.9044: build.mk's Talon_GenRegMgr) into its own
# talon: directory (progress/DEdit_map.md).

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "Talon_GenRegMgr - Win32 Release"

# PROP BASE Use_MFC 1
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 1
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\built\ReleaseTalon\GenRegMgr"
# PROP Intermediate_Dir "..\built\ReleaseTalon\GenRegMgr"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "..\stl" /I "..\incs" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "NO_PRAGMA_LIBS" /YX /FD /c
# ADD BASE RSC /l 0x409
# ADD RSC /l 0x409
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\built\ReleaseTalon\GenRegMgr\genregmgr.lib"

!ENDIF 

# Begin Target

# Name "Talon_GenRegMgr - Win32 Release"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;hpj;bat;for;f90"
# Begin Source File

SOURCE=.\GenRegMgr.cpp
# End Source File
# End Group
# End Target
# End Project

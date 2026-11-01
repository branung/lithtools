# Microsoft Developer Studio Project File - Name="Talon_ButeMgr" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=Talon_ButeMgr - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "ButeMgr_Talon.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "ButeMgr_Talon.mak" CFG="Talon_ButeMgr - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "Talon_ButeMgr - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE 

# talon: ButeMgr as the Talon DEdit (2001-12-06) links it: compiled by VC6 SP5
# talon: with the Processor Pack (cl 12.00.9044: build.mk's Talon_ButeMgr), /O2,
# talon: with MFC (its strings are CStrings) linked statically, the LT2
# talon: STLport and the LT2 StdLith and Lith headers, which the 2001 DEdit
# talon: links (progress/DEdit_map.md). Build 51's sources, the 2001 ones'
# talon: near relatives.

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "Talon_ButeMgr - Win32 Release"

# PROP BASE Use_MFC 1
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 1
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\built\ReleaseTalon\ButeMgr"
# PROP Intermediate_Dir "..\built\ReleaseTalon\ButeMgr"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "..\..\LT2\lithshared\stl" /I "..\..\LT2\lithshared\stdlith" /I "..\cryptmgr" /I "..\..\LT2\lithshared\lith" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /YX /FD /c
# ADD BASE RSC /l 0x409
# ADD RSC /l 0x409
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\built\ReleaseTalon\ButeMgr\ButeMgr.lib"

!ENDIF 

# Begin Target

# Name "Talon_ButeMgr - Win32 Release"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;hpj;bat;for;f90"
# Begin Source File

SOURCE=.\ARange.cpp
# End Source File
# Begin Source File

SOURCE=.\AVector.cpp
# End Source File
# Begin Source File

SOURCE=.\ButeMgr.cpp
# End Source File
# End Group
# End Target
# End Project

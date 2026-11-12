# build/build.mk -- every compile and link of the tools, as one Makefile.
#
# Adapted from cjacksrc's build/build.mk (Contract J.A.C.K.): the same
# machinery, with Visual C++ 6 in place of VC .NET 2003.
#
# The workspaces (VC6 .dsw/.dsp, written for this project: Monolith's own
# project files for these tools are lost) as Developer Studio would build
# them: tools/dsp2mk.py reads each workspace and its project files and
# writes the rules (.build/*.mk) -- every compile with the flags its
# project's "Release" configuration gives it, one source per compile, every
# library and link, the custom build steps. `make -j` then runs the compiles
# at once, and a rebuild redoes only what changed: an object is rebuilt when
# its source, a header it includes (tools/mkdeps.py finds them), or its
# command line changes (the line is kept in <object>.cmd), a binary when any
# of its inputs does.
#
# Run from legacy/talon/ (the build container mounts it at /repo):
#
#   make -f build/build.mk -j<N> [target...]
#
# Targets: all (the default: every workspace's wanted projects, below); a
# project by its workspace name; any single output.
#
# The compilers see src/ as drive S: and ext/toolchains/ as T:.
#
# From the environment (empty for the scored build):
#   TALON_CFLAGS   appended to every compile (/Z7 ...)
#   TALON_LDFLAGS  put first on every link
#   TALON_RCFLAGS  put first on every resource compile

ROOT := $(CURDIR)
# the submodules (../../ext: /ext in the container, the Makefile); RTOOLS
# the reconstruction tools (TOOLS is the tools this builds)
EXT := $(abspath $(ROOT)/../../ext)
RTOOLS := $(EXT)/tools
SHELL := /bin/bash
.SUFFIXES:
.DELETE_ON_ERROR:
MAKEFLAGS += --no-builtin-rules --no-builtin-variables
.DEFAULT_GOAL := all

ifeq ($(wildcard $(ROOT)/src/Engine),)
$(error run build/build.mk from legacy/talon/)
endif

empty :=
space := $(empty) $(empty)
comma := ,
tab := $(empty)	$(empty)
define nl


endef
# one shell word per make word, quoted: Windows paths keep their backslashes
q = $(foreach w,$(1),'$(subst ','\'',$(w))')

TALON_CFLAGS  ?=
TALON_LDFLAGS ?=
TALON_RCFLAGS ?=

# The workspaces, each with the projects of it to build (and, through the
# workspace, every library they depend on). A workspace that is not in the
# tree yet is skipped: the tools are reconstructed one binary at a time
# (docs/PLAN.md), and each binary's target in progress/targets.py is
# optional until its workspace is here.
#   src/LT2/lithshared/lithshared.dsw      the LT2 shared libraries (2001)
#   src/LT2/tools/PreProcessor/Processor.dsw  the world processor (AvP2)
#   src/Libs/Libs.dsw                      Monolith's shared static libraries
#   src/Engine/tools/Plugins/Plugins.dsw   the modelling-package plug-ins
#   src/LT2/tools/LithRez/LithRez.dsw      LithRez 1.00 (1998), Visual C++ 5.0
#   src/LT2/tools/PreProcessorNOLF/Processor.dsw  the world processor (NOLF),
#                                          Intel C/C++ 4.0 and VC6 SP3
#   src/LT2/tools/ModelEdit/ModelEdit.dsw  the model editor (Talon), VC6 SP5
#                                          with the Processor Pack
#   src/LT2/tools/DEdit/DEdit.dsw          the world editor (Talon), likewise
WORKSPACES := \
    src/LT2/lithshared/lithshared.dsw:LT2_StdLith,LT2_ZLib,LT2_RegMgr \
    src/LT2/tools/PreProcessor/Processor.dsw:Processor \
    src/LT2/tools/PreProcessorNOLF/Processor.dsw:ProcessorNOLF \
    src/LT2/tools/ModelEdit/ModelEdit.dsw:ModelEdit \
    src/LT2/tools/DEdit/DEdit.dsw:DEdit \
    src/Libs/Libs.dsw:ButeMgr,CryptMgr,DibMgr,GenRegMgr,Lib_Lith,MFCStub,RegMgr,regmgr32,Lib_StdLith,Lib_ZLib \
    src/Engine/tools/Plugins/Plugins.dsw:MayaDTXLoader,MaxDTXLoader30,MaxDTXLoader40,MaxModelExport30,MaxModelExport40,MaxWorldExport30,MaxWorldExport40,MaxWorldImport30,MaxWorldImport40,MayaModelExport30,MayaModelExport40,MayaWorldExport30,MayaWorldExport40 \
    src/LT2/tools/LithRez/LithRez.dsw:LithRez

# ---------------------------------------------------------------------------
# Wine and the toolchains
export WINEDEBUG := -all
export WINEDLLOVERRIDES := mscoree,mshtml=
WINE := wine
PFX := $(ROOT)/.wine/drives

# The shipped images' Rich headers (progress/COMPILERS.md): the plug-ins,
# Processor.exe and the libraries are cl 12.00.8966 (VC6 SP5); DEdit and
# ModelEdit 12.00.9044 (SP5 with the Processor Pack: CL_<project>); all link
# with link 6.00. msvc-6.00-8966 is SP5, with its CRT and MFC 4.2 (whose
# static library nafxcw.lib is the build 8569 the images' MFC objects show).
TC      := $(EXT)/toolchains
VC6     := $(TC)/msvc-6.00-8966
VC6PP   := $(TC)/msvc-6.00-9044
CL      := $(VC6)/BIN/cl.exe
LINK    := $(VC6)/BIN/link.exe
LIBEXE  := $(VC6)/BIN/lib.exe
# VC6's resource compiler is Developer Studio's (Common/MSDev98/Bin, with
# RCDLL.DLL): the processor-pack toolchains have none, so SP6's
RC      := $(TC)/msvc-6.00-9782/Common/MSDev98/Bin/RC.EXE

# Windows paths: src/ is S:, ext/toolchains/ T:, the rest Z:
wz = z:$(subst /,\,$(1))
ws = $(if $(filter $(ROOT)/src $(ROOT)/src/%,$(1)),s:$(subst /,\,$(patsubst $(ROOT)/src%,%,$(1))),$(if $(filter $(TC) $(TC)/%,$(1)),t:$(subst /,\,$(patsubst $(TC)%,%,$(1))),$(call wz,$(1))))

# The environment of a tool run
ENV_GAME := WINEPREFIX=$(PFX) WINEPATH='$(call ws,$(VC6))\BIN' \
    INCLUDE='$(call ws,$(VC6))\INCLUDE;$(call ws,$(VC6))\MFC\INCLUDE;$(call ws,$(VC6))\ATL\INCLUDE' \
    LIB='$(call ws,$(VC6))\LIB;$(call ws,$(VC6))\MFC\LIB'

# LithRez (lithrez.exe, 1998) is Visual C++ 5.0 with Service Pack 3
# (cl 11.00.7303, link 5.10.7303; progress/targets.py): a project built with
# it sets ENV_/CL_/LINK_/LIBEXE_<project> to these.
VC5     := $(TC)/msvc-5.00-7303
ENV_VC5 := WINEPREFIX=$(PFX) WINEPATH='$(call ws,$(VC5))\BIN' \
    INCLUDE='$(call ws,$(VC5))\INCLUDE' LIB='$(call ws,$(VC5))\LIB'
CL_VC5     := $(VC5)/BIN/cl.exe
LINK_VC5   := $(VC5)/BIN/link.exe
LIBEXE_VC5 := $(VC5)/BIN/lib.exe
$(foreach p,LithRez RezMgr Lith,$(eval ENV_$(p) := $$(ENV_VC5))$(eval CL_$(p) := $$(CL_VC5))\
    $(eval LINK_$(p) := $$(LINK_VC5))$(eval LIBEXE_$(p) := $$(LIBEXE_VC5)))

# NOLF Tools 001's Processor.exe (2001-01-03; progress/Processor_nolf_map.md):
# its own objects are Intel C/C++ 4.0's (icl-4.0, with VC6 SP3's headers and
# MFC's), but PreWorld.cpp, which is VC6 SP3's (cl 12.00.8447), as are the
# LT2 libraries (the NOLF_* projects) and the MFC and C runtime it links
# statically (msvc-6.00-8447's nafxcw.lib and libcmt.lib), with SP3's linker.
VC6SP3  := $(TC)/msvc-6.00-8447
ICL40   := $(TC)/icl-4.0
ENV_SP3 := WINEPREFIX=$(PFX) WINEPATH='$(call ws,$(VC6SP3))\BIN' \
    INCLUDE='$(call ws,$(VC6SP3))\INCLUDE;$(call ws,$(VC6SP3))\MFC\INCLUDE' \
    LIB='$(call ws,$(VC6SP3))\LIB;$(call ws,$(VC6SP3))\MFC\LIB'
ENV_ICL40 := WINEPREFIX=$(PFX) WINEPATH='$(call ws,$(ICL40))\BIN' \
    INCLUDE='$(call ws,$(ICL40))\INCLUDE;$(call ws,$(VC6SP3))\INCLUDE;$(call ws,$(VC6SP3))\MFC\INCLUDE' \
    LIB='$(call ws,$(ICL40))\LIB;$(call ws,$(VC6SP3))\LIB;$(call ws,$(VC6SP3))\MFC\LIB'
$(foreach p,NOLF_StdLith NOLF_ZLib NOLF_RegMgr ProcessorNOLF__PreWorld,$(eval ENV_$(p) := $$(ENV_SP3))\
    $(eval CL_$(p) := $$(VC6SP3)/BIN/cl.exe)$(eval LIBEXE_$(p) := $$(VC6SP3)/BIN/lib.exe))
ENV_ProcessorNOLF  := $(ENV_ICL40)
CL_ProcessorNOLF   := $(ICL40)/BIN/icl.exe
LINK_ProcessorNOLF := $(VC6SP3)/BIN/link.exe

# The Talon ModelEdit (2001-12-19; progress/ModelEdit_map.md): its own
# objects and the LT2 libraries it links (LT2_StdLith_alloc,
# LT2_RegMgr_Talon) are VC6 SP5 with the Processor Pack's (cl 12.00.9044,
# Rich 0x31/9044), with that toolchain's headers, MFC (nafxcw.lib, build
# 8569) and C runtime, linked statically. The Talon DEdit (2001-12-06;
# progress/DEdit_map.md) likewise, with the 2001 libraries it links
# (LT2_ZLib_Talon and the Talon_* projects).
ENV_VC6PP := WINEPREFIX=$(PFX) WINEPATH='$(call ws,$(VC6PP))\BIN' \
    INCLUDE='$(call ws,$(VC6PP))\INCLUDE;$(call ws,$(VC6PP))\MFC\INCLUDE' \
    LIB='$(call ws,$(VC6PP))\LIB;$(call ws,$(VC6PP))\MFC\LIB'
$(foreach p,ModelEdit DEdit LT2_StdLith_alloc LT2_RegMgr_Talon LT2_ZLib_Talon Talon_ButeMgr Talon_CryptMgr \
    Talon_DibMgr Talon_GenRegMgr Talon_LTAMgr,$(eval ENV_$(p) := $$(ENV_VC6PP))$(eval CL_$(p) := $$(VC6PP)/BIN/cl.exe)\
    $(eval LINK_$(p) := $$(VC6PP)/BIN/link.exe)$(eval LIBEXE_$(p) := $$(VC6PP)/BIN/lib.exe))
# DEdit includes <afxpriv.h>, whose <afxconv.h> needs ATL's <atlconv.h>:
# the Processor Pack toolchain has no ATL, so Developer Studio's (ATL 3.0,
# msvc-6.00-9782) follows.
ENV_DEdit := $(ENV_VC6PP) INCLUDE='$(call ws,$(VC6PP))\INCLUDE;$(call ws,$(VC6PP))\MFC\INCLUDE;$(call ws,$(TC))\msvc-6.00-9782\VC98\ATL\Include'

# The prefix: made the first time, then only checked; every tool run waits
# for it (an order-only prerequisite, so it causes no rebuild).
$(PFX)/.ready:
	@echo "   WINE prefix $(@D)"
	@mkdir -p $(@D) && export WINEPREFIX=$(@D) && { $(WINE) wineboot -i >/dev/null 2>&1; \
	  for v in TEMP TMP; do $(WINE) reg add 'HKCU\Environment' /v $$v /t REG_SZ /d 'C:\windows\temp' /f >/dev/null 2>&1; done; \
	  wineserver -w; } && test -f $(@D)/system.reg && touch $@
.PHONY: wine-drives FORCE
wine-drives: $(PFX)/.ready
	@ln -sfn $(ROOT)/src $(PFX)/dosdevices/s:
	@ln -sfn $(TC) $(PFX)/dosdevices/t:

# ---------------------------------------------------------------------------
# The machinery
#
# Every output's command is a variable, cmd_<output>, that the rule runs and
# keeps in <output>.cmd. An output whose command differs from the kept one
# is rebuilt (FORCE): a changed flag, a file added to a link.
cmd_changed = $(call str_ne,$(strip $(cmd_$(1))),$(strip $(file <$(1).cmd)),FORCE)
str_ne = $(if $(subst x$(1),,x$(2))$(subst x$(2),,x$(1)),$(3))

OBJECTS :=
DEPSPEC :=
TOOLS :=

# $(call compile,<object>,<working dir>,<source as spelled>,<env>,<exe>,<flags>,<prefix>)
define compile
OBJECTS += $(1)
cmd_$(1) := cd $(2) && $(4) $(WINE) $(5) $(call q,$(6) $(3) $(TALON_CFLAGS))
DEPSPEC += $(subst $(space),|,$(1)$(tab)$(2)$(tab)$(3)$(tab)$(strip $(6)))
$(1): $$(call cmd_changed,$(1)) | $(7)
endef

# $(call tool,<output>,<prerequisites>,<command>): any other build step
define tool
cmd_$(1) := $(3)
$(1): $$(call cmd_changed,$(1)) $(2)
TOOLS += $(1)
endef

# $(call rcfile,<res>,<project dir>,<rc as spelled>,<rc in the tree>,<flags>)
define rcfile
DEPSPEC += $(subst $(space),|,$(1)$(tab)$(2)$(tab)$(3)$(tab)$(strip $(TALON_RCFLAGS) $(5)))
$(call tool,$(1),$(4) | wine-drives,\
    cd $(2) && $(ENV_GAME) $(WINE) $(RC) $(TALON_RCFLAGS) $(call q,$(5) $(3)))
endef

# $(call library,<lib>,<project dir>,<flags>,<objects>,<objects as the project dir sees them>,
#   <env>,<lib.exe>)
define library
$(call tool,$(1),$(4) | wine-drives,\
    cd $(2) && { printf '%s\n' $(call q,$(3) $(subst /,\,$(5))) > $(1:%=$(ROOT)/%).rsp; } && \
    $(6) $(WINE) $(7) '@$(call ws,$(ROOT)/$(1)).rsp')
endef

# $(call binary,<dll or exe>,<project dir>,<flags>,<prerequisites>,<inputs as the project dir sees them>,
#   <the libraries' precompiled-header objects, as the project dir sees them>,<env>,<link.exe>)
# Through a response file, as Developer Studio links; with a map beside it.
Z7 := $(findstring /Z7,$(TALON_CFLAGS))
define binary
$(call tool,$(1),$(4) | wine-drives,\
    cd $(2) && { printf '%s\n' $(call q,$(TALON_LDFLAGS) $(3) /map:$(call ws,$(ROOT)/$(basename $(1))).map $(subst /,\,$(5) $(if $(Z7),$(6)))) > $(1:%=$(ROOT)/%).rsp; } && \
    $(7) $(WINE) $(8) '@$(call ws,$(ROOT)/$(1)).rsp')
endef

# $(call custom,<outputs>,<input>,<project dir>,<commands>): a source's
# custom build step, its commands run by cmd.exe
define custom
$(1) &: $(2) | wine-drives
	@echo "   GEN  $(1)"
	@cd $(3) && $(ENV_GAME) $(WINE) cmd /c $(call q,$(4)) >/dev/null
	@for o in $(1); do [ -f "$$$$o" ] || { f=$$$$(find "$$$$(dirname $$$$o)" -maxdepth 1 -iname "$$$$(basename $$$$o)" | head -1); \
	  [ -n "$$$$f" ] && mv "$$$$f" "$$$$o"; }; test -f "$$$$o" || { echo "error: $$$$o was not made" >&2; exit 1; }; done
endef

# ---------------------------------------------------------------------------
# The projects (tools/dsp2mk.py)

DEPDIR := $(ROOT)/.build
$(shell mkdir -p $(DEPDIR))
PROJECTS :=
WANTED :=
define workspace
ifneq ($(wildcard $(ROOT)/$(1)),)
DSP_OK := $$(shell python3 $(RTOOLS)/dsp2mk.py --have "$$(PROJECTS)" $(1) Release $(DEPDIR)/$(basename $(notdir $(1))).mk $(subst $(comma), ,$(2)) 2>$(DEPDIR)/dsp2mk-$(basename $(notdir $(1))).log && echo ok)
ifneq ($$(DSP_OK),ok)
$$(error tools/dsp2mk.py failed on $(1): $$(file <$(DEPDIR)/dsp2mk-$(basename $(notdir $(1))).log))
endif
include $(DEPDIR)/$(basename $(notdir $(1))).mk
WANTED += $$(filter $$(PROJECTS),$(subst $(comma), ,$(2)))
endif
endef
$(foreach w,$(WORKSPACES),$(eval $(call workspace,$(firstword $(subst :, ,$(w))),$(lastword $(subst :, ,$(w))))))

.PHONY: all
all: $(WANTED)

# ---------------------------------------------------------------------------
# The rules all compiles and tools share

dups := $(strip $(foreach o,$(sort $(OBJECTS)),$(if $(word 2,$(filter $(o),$(OBJECTS))),$(o))))
ifneq ($(dups),)
$(error two sources compile to one object: $(dups))
endif

# Under Wine, a compile that uses a precompiled header now and then dies
# with C1001 or C1060 while other compiles run, and now and then wineserver
# itself goes away under a parallel build ("wine client error: ...
# Connection reset by peer"; the next wine starts a new one); the same
# compile run again succeeds, and gives the same object. Those are retried,
# up to three tries in all; any other error fails the build at once.
$(OBJECTS):
	@echo "   CC   $@"
	@mkdir -p $(@D) && rm -f $@
	@for try in 1 2 3; do \
	  out=$$( { $(cmd_$@); } 2>&1 ); rc=$$?; \
	  if [ $$rc = 0 ] || [ $$try = 3 ] || ! grep -qE 'error C10(01|60)|wine client error' <<< "$$out"; then printf '%s\n' "$$out"; break; fi; \
	  echo "   CC   $@: $$(grep -oE 'error C10(01|60)|wine client error' <<< "$$out" | head -1), compiling it again"; rm -f $@; \
	done; exit $$rc
	@test -f $@ || { echo "error: $@ was not built" >&2; exit 1; }
	@printf '%s\n' '$(subst ','\'',$(cmd_$@))' > $@.cmd

$(TOOLS):
	@echo "   GEN  $@"
	@mkdir -p $(@D) && rm -f $@
	@$(cmd_$@)
	@test -f $@ || { echo "error: $@ was not built" >&2; exit 1; }
	@printf '%s\n' '$(subst ','\'',$(cmd_$@))' > $@.cmd

FORCE:

# Header dependencies (tools/mkdeps.py), brought up to date on every run
$(file >$(DEPDIR)/deps.spec,$(subst |, ,$(subst $(space),$(nl),$(DEPSPEC))))
DEPS_OK := $(shell python3 $(RTOOLS)/mkdeps.py $(DEPDIR)/deps.spec $(DEPDIR)/deps.mk && echo ok)
ifneq ($(DEPS_OK),ok)
$(error tools/mkdeps.py failed)
endif
include $(DEPDIR)/deps.mk

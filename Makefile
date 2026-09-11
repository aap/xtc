CC=ee-gcc
CXX=ee-g++

# Every program under demos/ is built into an ELF of its own, with both
# toolchains, out of the same library and skeleton objects:
#
#	src/		the PS2 backend and its microcode -- the library
#	common/		the portable model/anim/draw layer, both backends
#	skeleton/	the program layer: pad, host files, frame loop
#	demos/NAME/	one program; its ELF is NAME.elf / NAME_freesce.elf
#
# The ELFs land in the repo root because host: paths resolve next to the
# ELF and the scenes want ./build/chk and ./samples from here.
#
# Adding a demo: make the directory with a .c in it and copy the four
# link rules below (two toolchains x two lines).  The SDK's make is too
# old for $(eval), so the rules are spelled out rather than generated.
# The list is whatever directories are there, so a demo that is not
# checked in (spyro, while its assets are sorted out) builds when
# present and is not missed when absent.
DEMOS := $(sort $(notdir $(patsubst %/,%,$(dir $(wildcard demos/*/*.c)))))

# the SDK's make is too old for order-only prerequisites, so the chunks
# the demos load are a sibling goal rather than one of the ELFs'
all: $(DEMOS:%=%.elf) chunks

SRCDIRS := src src/vu1 src/data skeleton common
DEMODIRS := $(DEMOS:%=demos/%)
OBJDIR := build

# mdma is a sibling checkout, built with our compiler and our flags:
# MDMA_DEBUG follows NDEBUG per translation unit, so a library and a caller
# that disagree would disagree about the __FILE__/__LINE__ arguments too.
MDMADIR := ../mdma
MDMASRC := $(MDMADIR)/mdma.c $(MDMADIR)/mdmadis.c
MDMAOBJ := $(addprefix $(OBJDIR)/mdma/,$(notdir $(MDMASRC:.c=.o)))

SCELIBDIR := /usr/local/sce/ee/lib

# the library and skeleton, shared by every demo
CXXSRC := $(foreach dir,$(SRCDIRS),$(wildcard $(dir)/*.cpp))
CSRC := $(foreach dir,$(SRCDIRS),$(wildcard $(dir)/*.c))
VUSRC := $(foreach dir,$(SRCDIRS),$(wildcard $(dir)/*.dsm))

# and one demo's own sources.  $(call demosrc,NAME), $(call demoobj,DIR,NAME)
demosrc = $(wildcard demos/$(1)/*.c) $(wildcard demos/$(1)/*.cpp) \
	$(wildcard demos/$(1)/*.dsm)
demoobj = $(addprefix $(1)/,$(patsubst %.dsm,%.o,$(patsubst %.cpp,%.o,\
	$(patsubst %.c,%.o,$(call demosrc,$(2))))))

DEMOSRC := $(foreach d,$(DEMOS),$(call demosrc,$(d)))

#CFLAGS := -fno-common -fno-exceptions
CFLAGS := -std=gnu99 -Os -fno-common -fno-exceptions ####-ffunction-sections -fdata-sections
CXXFLAGS := -Os -fno-common -fno-exceptions -fno-rtti

# libmc and libscf were in here and nothing references them
LIBS=	$(SCELIBDIR)/libgraph.a	\
	$(SCELIBDIR)/libdma.a	\
	$(SCELIBDIR)/libpc.a	\
	$(SCELIBDIR)/libpad.a	\
	$(SCELIBDIR)/libcdvd.a


GCCLIB := /usr/local/sce/ee/gcc/lib/gcc-lib/ee/3.2-ee-040921
CRT_BEGIN := build/crt0.o $(GCCLIB)/crti.o $(GCCLIB)/crtbegin.o
CRT_END := $(GCCLIB)/crtend.o $(GCCLIB)/crtn.o

OBJ := $(addprefix $(OBJDIR)/,$(CSRC:.c=.o) $(CXXSRC:.cpp=.o) $(VUSRC:.dsm=.o))
DEP := $(addprefix $(OBJDIR)/,$(CSRC:.c=.d) $(CXXSRC:.cpp=.d) \
	$(filter %.d,$(DEMOSRC:.c=.d) $(DEMOSRC:.cpp=.d)))

ASINC := $(addprefix -I,$(SRCDIRS))
INC := $(addprefix -I,$(SRCDIRS)) -Icommon	\
	-I$(MDMADIR)			\
	-I/usr/local/sce/common/include	\
	-I/usr/local/sce/ee/include

# what every ELF is made of besides the demo's own objects.
# $(call linksce,the demo's objects)
COMMONOBJ := $(OBJ) $(MDMAOBJ)
linksce = $(CXX) -o $@ $(CRT_BEGIN) $(COMMONOBJ) $(1) $(LIBS) $(CRT_END) \
	-T /usr/local/sce/ee/lib/app.cmd -L/usr/local/sce/ee/lib -lm -nostartfiles

XTCDEMO_OBJ := $(call demoobj,$(OBJDIR),xtcdemo)
SPYRO_OBJ := $(call demoobj,$(OBJDIR),spyro)
FOX_OBJ := $(call demoobj,$(OBJDIR),fox)

xtcdemo.elf: build/crt0.o $(COMMONOBJ) $(XTCDEMO_OBJ)
	$(call linksce,$(XTCDEMO_OBJ))
spyro.elf: build/crt0.o $(COMMONOBJ) $(SPYRO_OBJ)
	$(call linksce,$(SPYRO_OBJ))
fox.elf: build/crt0.o $(COMMONOBJ) $(FOX_OBJ)
	$(call linksce,$(FOX_OBJ))

run: xtcdemo.elf
	dsedb -r run xtcdemo.elf

build/crt0.o:
	@mkdir -p $(@D)
	$(CC) -c -xassembler-with-cpp -o $@ $(SCELIBDIR)/crt0.s

# chunks: models written as assembler source by tools/xm2dsm.lua and
# linked into the file the loader wants (tools/chunk.inc, tools/chk.ld).
# per target like every chunk, but the same for both toolchains.
#
# A .xm may come from samples/ (the tracked assets) or from local/, which
# is gitignored and holds whatever the machine happens to have -- game
# assets that must not end up in the repo, for one.  Each source gets its
# own pattern rule below; make picks the one whose .xm exists.  local/ is
# wildcarded so `make chunks' still works when it is not there.
CHKDIR := build/chk

# demos/spyro is a viewer of all 35 levels, so there are 70 of these:
# local/spyro/levels/levelNN/{world,sky}.xm -> build/chk/spyro/
# levelNN_{world,sky}.chk.  The directory is one level's worth of the
# name so a pattern rule can do it, and the demo builds the path the
# same way.  `make -j8 chunks' if you are in a hurry -- it is 70 runs
# each of the stripper, lua, dvp-as and ld.
SPYROLVL := $(patsubst local/spyro/levels/%/world.xm,%,\
	$(wildcard local/spyro/levels/*/world.xm))
SPYROSKY := $(patsubst local/spyro/levels/%/sky.xm,%,\
	$(wildcard local/spyro/levels/*/sky.xm))
SPYROCHK := $(SPYROLVL:%=$(CHKDIR)/spyro/%_world.chk) \
	$(SPYROSKY:%=$(CHKDIR)/spyro/%_sky.chk)

CHUNKS := $(CHKDIR)/fox.chk $(CHKDIR)/fox_anim.chk $(SPYROCHK)

# the clips demos/fox plays: four standing idles, three sitting, three
# lying, the alert crouch, and the eight Trans_* clips that get between
# those four poses.  All of them keep the fox where it is -- the
# locomotion clips animate RigRoot and would walk it out of frame.
# samples/ is a submodule, so the cut is made here and not in there.
FOXCLIPS := \
	A4_Stand_Breathing_01		\
	A1_Stand_Idle_02		\
	A3_Stand_Idle_01		\
	A2_Stand_Eating_01		\
	Sitting_Breathing_01		\
	Sitting_Idle_01			\
	Sitting_Idle_02			\
	Lying_Breathing_01		\
	Lying_Idle_01			\
	Lying_Idle_02			\
	A5_StandAngry_Breathing_01	\
	Trans_Stand_to_Sitting		\
	Trans_Sitting_to_Stand		\
	Trans_Stand_to_Lying		\
	Trans_Lying_to_Stand		\
	Trans_Sitting_to_Lying		\
	Trans_Lying_to_Sitting		\
	Trans_Stand_to_StandAngry	\
	Trans_StandAngry_to_Stand
# -nogeo drops the xGeometry from the chunk (then the bounding sphere is
# a guess), see tools/xm2dsm.lua
CHKFLAGS :=

chunks: $(CHUNKS) $(CHKDIR)/fox_anim.xan

# keep the strips and the source around for a look
# keep the fox's strips and source around for a look; the 70 level
# chunks' intermediates are half a gigabyte and go
.SECONDARY: $(CHKDIR)/fox.strips $(CHKDIR)/fox.dsm $(CHKDIR)/fox.o $(CHKDIR)/fox_anim.dsm $(CHKDIR)/fox_anim.o

# the stripper runs on the host
HOSTCXX := g++
HOSTCC := gcc
build/host/xstrip: tools/xstrip.cpp common/tristrip.cpp common/tristrip.h
	@mkdir -p $(@D)
	$(HOSTCXX) -O2 -Icommon -Isrc_gl -o $@ tools/xstrip.cpp common/tristrip.cpp

# spyroconv turns a Spyro the Dragon (PS1) WAD straight into xtc assets,
# see demos/spyro/import.sh
build/host/spyroconv: tools/spyroconv.c src/lodepng.c src/lodepng.h
	@mkdir -p $(@D)
	$(HOSTCC) -O2 -Isrc -o $@ tools/spyroconv.c src/lodepng.c -lm

# the converter reads the batch size and the input layout from the
# microcode, but the chunks must not depend on the microcode files:
# every edit to the code would regenerate all the geometry.  So the
# interface is extracted -- the equates the batch size comes from and
# the input descriptor -- into a file that is only touched when that
# changes, and the chunks depend on the file.
PIPEINFO := $(CHKDIR)/pipeinfo.txt
XM2DSMDEP := tools/xm2dsm.lua $(PIPEINFO)

$(PIPEINFO): src/vu1/stdPipe.dsm
	@mkdir -p $(@D)
	@(grep -E '^\.equ (numInAttribs|numOutAttribs|numOutBuf|numSkinAttribs|(std|skin)_(vertexTop|numUnpackAttribs|vertCount)),' $<; \
	  sed -n '/^std_inputDesc:/,/^$$/p' $<; sed -n '/^skin_inputDesc:/,/^$$/p' $<) > $@.tmp
	@if cmp -s $@.tmp $@; then rm $@.tmp; echo "pipeinfo unchanged"; else mv $@.tmp $@; echo "pipeinfo changed"; fi

$(CHKDIR)/%.strips: samples/fox/%.xm build/host/xstrip
	@mkdir -p $(@D)
	build/host/xstrip -o $@ $<

$(CHKDIR)/%.dsm: samples/fox/%.xm $(CHKDIR)/%.strips $(XM2DSMDEP)
	@mkdir -p $(@D)
	lua tools/xm2dsm.lua -pipes src/vu1 -strips $(CHKDIR)/$*.strips $(CHKFLAGS) $< > $@

# the 35 levels.  The stem is levelNN and the part is in the target's
# name, so world and sky want a rule each.  The samples/fox rules above
# would have to find samples/fox/spyro/levelNN_world.xm to be used for
# one of these, so there is nothing ambiguous about it.
$(CHKDIR)/spyro/%_world.strips: local/spyro/levels/%/world.xm build/host/xstrip
	@mkdir -p $(@D)
	build/host/xstrip -o $@ $<

$(CHKDIR)/spyro/%_world.dsm: local/spyro/levels/%/world.xm \
		$(CHKDIR)/spyro/%_world.strips $(XM2DSMDEP)
	@mkdir -p $(@D)
	lua tools/xm2dsm.lua -pipes src/vu1 \
		-strips $(CHKDIR)/spyro/$*_world.strips $(CHKFLAGS) $< > $@

$(CHKDIR)/spyro/%_sky.strips: local/spyro/levels/%/sky.xm build/host/xstrip
	@mkdir -p $(@D)
	build/host/xstrip -o $@ $<

$(CHKDIR)/spyro/%_sky.dsm: local/spyro/levels/%/sky.xm \
		$(CHKDIR)/spyro/%_sky.strips $(XM2DSMDEP)
	@mkdir -p $(@D)
	lua tools/xm2dsm.lua -pipes src/vu1 \
		-strips $(CHKDIR)/spyro/$*_sky.strips $(CHKFLAGS) $< > $@

# animation is the same idea without the DMA chains, so no strips and
# no microcode: tools/xan2dsm.lua straight from the .xan.  Its own
# pattern, so the %.dsm above does not go looking for a .xm -- and a
# pattern rather than the explicit rule this used to be, because make
# will not chain .chk <- .o <- .dsm through an explicit one, so
# `make chunks' on an empty build/chk never got past the .dsm.
$(CHKDIR)/%_anim.dsm: samples/fox/%.xan tools/xan2dsm.lua
	@mkdir -p $(@D)
	lua tools/xan2dsm.lua -name $*_anim $(addprefix -clip ,$(FOXCLIPS)) $< > $@

# the same clips as text: what the demo falls back to without a chunk,
# and what tools/xanchkdiff.py checks the chunk against
$(CHKDIR)/fox_anim.xan: samples/fox/fox.xan tools/xancut.py
	@mkdir -p $(@D)
	python3 tools/xancut.py $< $@ $(FOXCLIPS)

$(CHKDIR)/%.o: $(CHKDIR)/%.dsm tools/chunk.inc
	ee-dvp-as -Itools $< -o $@

$(CHKDIR)/%.chk: $(CHKDIR)/%.o tools/chk.ld
	ee-ld -T tools/chk.ld -o $@ $<

$(OBJDIR)/%.o: $(OBJDIR)/%.dsm_x
	@mkdir -p $(@D)
	ee-dvp-as -alm=$(@:.o=.lst) $(ASINC) -stalls-pipeline -no-fetching $< -o $@

# the microcode .dsm files #include the .vu/.inc files next to them
VUINC := $(wildcard src/vu1/*.vu) $(wildcard src/vu1/*.inc)

$(OBJDIR)/%.dsm_x: %.dsm $(VUINC)
	@mkdir -p $(@D)
	joinvu $< | cpp $(ASINC) | grep -v '^#' > $@

$(OBJDIR)/mdma/%.o: $(MDMADIR)/%.c $(MDMADIR)/mdma.h $(MDMADIR)/mdmaplat.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(OBJDIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@
$(OBJDIR)/%.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INC) -c $< -o $@

$(OBJDIR)/%.d: %.c
	@mkdir -p $(@D)
	$(CC) -MM -MT $(@:.d=.o) $(CFLAGS) $(INC) $< > $@
$(OBJDIR)/%.d: %.cpp
	@mkdir -p $(@D)
	$(CXX) -MM -MT $(@:.d=.o) $(CFLAGS) $(INC) $< > $@

# ---- freesce ---------------------------------------------------------------
# Two roots, because they are two different axes. FREESCE is the SDK vintage --
# headers, libraries, crt0, app.cmd -- and there is one per SDK version. The
# compiler is not SDK-versioned, so it has its own.
#
# Both point at a *tree root* with ee/include and ee/lib under it, which is how
# an install and a git worktree are both laid out. So either works:
#
#	make freesce
#	make freesce FREESCE=/u/aap/src/freesce_24      # straight from a worktree
#	make freesce FREESCE=/usr/local/freesce24       # a different vintage
#
# $(FREESCE) is already an environment variable here, so it is honoured
# without being passed. FREESCE_GCC deliberately does *not* derive from it: a
# source worktree has headers and libraries but no compiler, and the compiler
# is not SDK-versioned anyway, so it should not be copied into every vintage.
FREESCE     ?= /usr/local/freesce
FREESCE_GCC ?= /usr/local/freesce/ee/gcc

FREESCE_LIB := $(FREESCE)/ee/lib
FREESCE_INC := -I$(FREESCE)/ee/include
# libopad is the SDK 2.0 libpad, the last one that accepts the boot
# ROM's padman (XPADMAN is padman 3.6; 2.4 onwards demands 4.x, i.e.
# padman.irx).  JOY_ROMPAD in FSFLAGS goes with it and with nothing else.
FREESCE_LIBS = $(FREESCE_LIB)/libgraph.a $(FREESCE_LIB)/libdma.a \
	$(FREESCE_LIB)/libpc.a $(FREESCE_LIB)/libopad.a \
	$(FREESCE_LIB)/libcdvd.a $(FREESCE_LIB)/libkernl.a

# ---- freesce, end to end ---------------------------------------------------
# No /usr/local/sce anywhere: freesce's own ee-gcc 2.9, its newlib and libgcc,
# its ee-dvp-as, and freesce's SDK libraries, crt0 and app.cmd. The default
# target above is the other pairing -- SCE's 3.2 compiler with the 3.0 SDK,
# which is what that compiler is for.
#
# 2.9 is a C89 compiler and xtc's C is C99-flavoured -- declarations after
# statements, `for(uint32 i = ...)' -- so the sources are compiled as C++,
# which accepts all of that. That is what librw does on PS2 for the same
# reason. It does mean xtc's symbols are mangled in this build.
FSCC := $(FREESCE_GCC)/bin/ee-gcc
FSCXX := $(FREESCE_GCC)/bin/ee-g++
FSGCCLIB := $(FREESCE_GCC)/lib/gcc-lib/ee/2.9-ee-991111-01
FSFLAGS := -O2 -fno-common -fno-exceptions -DLODEPNG_NO_COMPILE_CPP -DJOY_ROMPAD
FSINC := $(addprefix -I,$(SRCDIRS)) -Icommon -I$(MDMADIR) $(FREESCE_INC)

FSOBJDIR := build/freesce
FSOBJ := $(addprefix $(FSOBJDIR)/,$(CSRC:.c=.o) $(CXXSRC:.cpp=.o) $(VUSRC:.dsm=.o)) \
	$(addprefix $(FSOBJDIR)/mdma/,$(notdir $(MDMASRC:.c=.o)))

FS_XTCDEMO_OBJ := $(call demoobj,$(FSOBJDIR),xtcdemo)
FS_SPYRO_OBJ := $(call demoobj,$(FSOBJDIR),spyro)
FS_FOX_OBJ := $(call demoobj,$(FSOBJDIR),fox)

freesce: $(DEMOS:%=%_freesce.elf) chunks

# Linked with the C driver: the objects are C++ but nothing needs the C++
# runtime (no exceptions, no new, no dynamic initialisers), and ee-g++ would
# add a -lstdc++ that this toolchain does not have.
#
# No crtbegin/crtend either: gcc 2.9 has none for this target and puts __main,
# __do_global_ctors and __do_global_dtors together in libgcc's __main.o, which
# collides with the __main in freesce's crtbegin.o.
#
# $(call linkfs,the demo's objects)
linkfs = $(FSCC) -o $@ $(FREESCE_LIB)/crt0.o $(FREESCE_LIB)/crti.o \
	    $(FSOBJ) $(1) \
	    -Wl,--start-group $(FREESCE_LIBS) -lc -lm -lgcc -Wl,--end-group \
	    $(FREESCE_LIB)/crtn.o \
	    -T $(FREESCE_LIB)/app.cmd -L$(FREESCE_LIB) -nostartfiles

xtcdemo_freesce.elf: $(FSOBJ) $(FS_XTCDEMO_OBJ)
	$(call linkfs,$(FS_XTCDEMO_OBJ))
spyro_freesce.elf: $(FSOBJ) $(FS_SPYRO_OBJ)
	$(call linkfs,$(FS_SPYRO_OBJ))
fox_freesce.elf: $(FSOBJ) $(FS_FOX_OBJ)
	$(call linkfs,$(FS_FOX_OBJ))

# freesce's ee-dvp-as is binutils 2.9 and has neither -stalls-pipeline nor
# -no-fetching. Both are warning options -- hazard reporting, not codegen --
# so the object is the same; you just don't get told about hazards here.
$(FSOBJDIR)/%.o: $(FSOBJDIR)/%.dsm_x
	@mkdir -p $(@D)
	$(FREESCE_GCC)/bin/ee-dvp-as -alm=$(@:.o=.lst) $(ASINC) $< -o $@

$(FSOBJDIR)/%.dsm_x: %.dsm $(VUINC)
	@mkdir -p $(@D)
	joinvu $< | cpp $(ASINC) | grep -v '^#' > $@

$(FSOBJDIR)/mdma/%.o: $(MDMADIR)/%.c $(MDMADIR)/mdma.h $(MDMADIR)/mdmaplat.h
	@mkdir -p $(@D)
	$(FSCXX) $(FSFLAGS) -x c++ $(FSINC) -c $< -o $@

$(FSOBJDIR)/%.o: %.c
	@mkdir -p $(@D)
	$(FSCXX) $(FSFLAGS) -x c++ $(FSINC) -c $< -o $@
$(FSOBJDIR)/%.o: %.cpp
	@mkdir -p $(@D)
	$(FSCXX) $(FSFLAGS) $(FSINC) -c $< -o $@

.PHONY: all run freesce chunks clean

clean:
	rm -rf build $(DEMOS:%=%.elf) $(DEMOS:%=%_freesce.elf)

-include $(DEP)

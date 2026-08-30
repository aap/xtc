CC=ee-gcc
CXX=ee-g++

TARGET=xtcdemo

SRCDIRS := src src/vu1
OBJDIR := build

# mdma is a sibling checkout, built with our compiler and our flags:
# MDMA_DEBUG follows NDEBUG per translation unit, so a library and a caller
# that disagree would disagree about the __FILE__/__LINE__ arguments too.
MDMADIR := ../mdma
MDMASRC := $(MDMADIR)/mdma.c $(MDMADIR)/mdmadis.c
MDMAOBJ := $(addprefix $(OBJDIR)/mdma/,$(notdir $(MDMASRC:.c=.o)))

SCELIBDIR := /usr/local/sce/ee/lib

CXXSRC := $(foreach dir,$(SRCDIRS),$(wildcard $(dir)/*.cpp))
CSRC := $(foreach dir,$(SRCDIRS),$(wildcard $(dir)/*.c))
VUSRC := $(foreach dir,$(SRCDIRS),$(wildcard $(dir)/*.dsm))

#CFLAGS := -fno-common -fno-exceptions
CFLAGS := -std=gnu99 -Os -fno-common -fno-exceptions ####-ffunction-sections -fdata-sections

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
DEP := $(addprefix $(OBJDIR)/,$(CSRC:.c=.d) $(CXXSRC:.cpp=.d))

ASINC := $(addprefix -I,$(SRCDIRS))
INC := $(addprefix -I,$(SRCDIRS))	\
	-I$(MDMADIR)			\
	-I/usr/local/sce/common/include	\
	-I/usr/local/sce/ee/include

$(TARGET).elf: build/crt0.o $(OBJ) $(MDMAOBJ)
	echo $(OBJ)
	$(CXX) -o $@ $(CRT_BEGIN) $(OBJ) $(MDMAOBJ) $(LIBS) $(CRT_END) -T /usr/local/sce/ee/lib/app.cmd -L/usr/local/sce/ee/lib -lm -nostartfiles -Wl,--gc-sections

run: $(TARGET).elf
	dsedb -r run $(TARGET).elf

build/crt0.o:
	@mkdir -p $(@D)
	$(CC) -c -xassembler-with-cpp -o $@ $(SCELIBDIR)/crt0.s

$(OBJDIR)/%.o: $(OBJDIR)/%.dsm_x
	@mkdir -p $(@D)
	ee-dvp-as -alm=$(@:.o=.lst) $(ASINC) -stalls-pipeline -no-fetching $< -o $@

$(OBJDIR)/%.dsm_x: %.dsm
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
	$(CXX) $(CFLAGS) $(INC) -c $< -o $@

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
FSINC := $(addprefix -I,$(SRCDIRS)) -I$(MDMADIR) $(FREESCE_INC)

FSOBJDIR := build/freesce
FSOBJ := $(addprefix $(FSOBJDIR)/,$(CSRC:.c=.o) $(CXXSRC:.cpp=.o) $(VUSRC:.dsm=.o)) \
	$(addprefix $(FSOBJDIR)/mdma/,$(notdir $(MDMASRC:.c=.o)))

freesce: $(TARGET)_freesce.elf

# Linked with the C driver: the objects are C++ but nothing needs the C++
# runtime (no exceptions, no new, no dynamic initialisers), and ee-g++ would
# add a -lstdc++ that this toolchain does not have.
#
# No crtbegin/crtend either: gcc 2.9 has none for this target and puts __main,
# __do_global_ctors and __do_global_dtors together in libgcc's __main.o, which
# collides with the __main in freesce's crtbegin.o.
$(TARGET)_freesce.elf: $(FSOBJ)
	$(FSCC) -o $@ $(FREESCE_LIB)/crt0.o $(FREESCE_LIB)/crti.o \
	    $(FSOBJ) \
	    -Wl,--start-group $(FREESCE_LIBS) -lc -lm -lgcc -Wl,--end-group \
	    $(FREESCE_LIB)/crtn.o \
	    -T $(FREESCE_LIB)/app.cmd -L$(FREESCE_LIB) -nostartfiles -Wl,--gc-sections

# freesce's ee-dvp-as is binutils 2.9 and has neither -stalls-pipeline nor
# -no-fetching. Both are warning options -- hazard reporting, not codegen --
# so the object is the same; you just don't get told about hazards here.
$(FSOBJDIR)/%.o: $(FSOBJDIR)/%.dsm_x
	@mkdir -p $(@D)
	$(FREESCE_GCC)/bin/ee-dvp-as -alm=$(@:.o=.lst) $(ASINC) $< -o $@

$(FSOBJDIR)/%.dsm_x: %.dsm
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

.PHONY: run freesce clean

clean:
	rm -rf build $(TARGET).elf $(TARGET)_freesce.elf

-include $(DEP)

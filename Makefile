CC=ee-gcc
CXX=ee-g++

TARGET=xtcdemo

SRCDIRS := src src/vu1
OBJDIR := build

SCELIBDIR := /usr/local/sce/ee/lib

CXXSRC := $(foreach dir,$(SRCDIRS),$(wildcard $(dir)/*.cpp))
CSRC := $(foreach dir,$(SRCDIRS),$(wildcard $(dir)/*.c))
VUSRC := $(foreach dir,$(SRCDIRS),$(wildcard $(dir)/*.dsm))

#CFLAGS := -fno-common -fno-exceptions
CFLAGS := -std=gnu99 -Os -fno-common -fno-exceptions ####-ffunction-sections -fdata-sections

LIBS=	$(SCELIBDIR)/libgraph.a	\
	$(SCELIBDIR)/libdma.a	\
	$(SCELIBDIR)/libmc.a	\
	$(SCELIBDIR)/libpc.a	\
	$(SCELIBDIR)/libpad.a	\
	$(SCELIBDIR)/libcdvd.a	\
	$(SCELIBDIR)/libscf.a


GCCLIB := /usr/local/sce/ee/gcc/lib/gcc-lib/ee/3.2-ee-030926
CRT_BEGIN := build/crt0.o $(GCCLIB)/crti.o $(GCCLIB)/crtbegin.o
CRT_END := $(GCCLIB)/crtend.o $(GCCLIB)/crtn.o

OBJ := $(addprefix $(OBJDIR)/,$(CSRC:.c=.o) $(CXXSRC:.cpp=.o) $(VUSRC:.dsm=.o))
DEP := $(addprefix $(OBJDIR)/,$(CSRC:.c=.d) $(CXXSRC:.cpp=.d))

ASINC := $(addprefix -I,$(SRCDIRS))
INC := $(addprefix -I,$(SRCDIRS))	\
	-I/usr/local/sce/common/include	\
	-I/usr/local/sce/ee/include

$(TARGET).elf: build/crt0.o $(OBJ)
	echo $(OBJ)
	$(CXX) -o $@ $(CRT_BEGIN) $(OBJ) $(LIBS) $(CRT_END) -T /usr/local/sce/ee/lib/app.cmd -L/usr/local/sce/ee/lib -lm -nostartfiles -Wl,--gc-sections

run: $(TARGET).elf
	dsedb -r run $(TARGET).elf

build/crt0.o:
	@mkdir -p $(@D)
	$(CC) -c -xassembler-with-cpp -o $@ /usr/local/sce/ee/lib/crt0.s

$(OBJDIR)/%.o: $(OBJDIR)/%.dsm_x
	@mkdir -p $(@D)
	ee-dvp-as -alm=$(@:.o=.lst) $(ASINC) -stalls-pipeline -no-fetching $< -o $@

$(OBJDIR)/%.dsm_x: %.dsm
	@mkdir -p $(@D)
	joinvu $< | cpp $(ASINC) | grep -v '^#' > $@

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

-include $(DEP)

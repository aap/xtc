#
# Cross Platform Makefile
# Compatible with MSYS2/MINGW, Ubuntu 14.04.1 and Mac OS X
#
# You will need GLFW (http://www.glfw.org):
# Linux:
#   apt-get install libglfw-dev
# Mac OS X:
#   brew install glfw
# MSYS2:
#   pacman -S --noconfirm --needed mingw-w64-x86_64-toolchain mingw-w64-x86_64-glfw
#

#CXX = g++
#CXX = clang++

EXE = xtc
IMGUI_DIR = /u/aap/othersrc/imgui
COMMON = ../common
SOURCES = main.cpp app.cpp conv.cpp xtc.cpp lua.cpp glad/glad.c lodepng/lodepng.cpp assimp_rw.cpp
SOURCES += $(COMMON)/xmodel.cpp $(COMMON)/xanim.cpp $(COMMON)/xdraw.cpp $(COMMON)/chunk.c $(COMMON)/tokenize.c
SOURCES += $(IMGUI_DIR)/imgui.cpp $(IMGUI_DIR)/imgui_demo.cpp $(IMGUI_DIR)/imgui_draw.cpp $(IMGUI_DIR)/imgui_tables.cpp $(IMGUI_DIR)/imgui_widgets.cpp
SOURCES += $(IMGUI_DIR)/backends/imgui_impl_glfw.cpp $(IMGUI_DIR)/backends/imgui_impl_opengl3.cpp
OBJS = $(addprefix build/, $(addsuffix .o, $(basename $(notdir $(SOURCES)))))
UNAME_S := $(shell uname -s)
LINUX_GL_LIBS = -lGL -lassimp $(LIBRWGTA)/lib/linux-amd64-null/Debug/librwgta.a $(LIBRW)/lib/linux-amd64-null/Debug/librw.a

CXXFLAGS = -DXTC_GL -I. -I$(COMMON) -I$(IMGUI_DIR) -I$(IMGUI_DIR)/backends -I$(LIBRW) -I$(LIBRWGTA) `pkg-config lua --cflags`
CXXFLAGS += -g -Wall -Wformat -Wno-multichar
LIBS = `pkg-config lua --libs`

##---------------------------------------------------------------------
## OPENGL ES
##---------------------------------------------------------------------

## This assumes a GL ES library available in the system, e.g. libGLESv2.so
# CXXFLAGS += -DIMGUI_IMPL_OPENGL_ES2
# LINUX_GL_LIBS = -lGLESv2

##---------------------------------------------------------------------
## BUILD FLAGS PER PLATFORM
##---------------------------------------------------------------------

ifeq ($(UNAME_S), Linux) #LINUX
	ECHO_MESSAGE = "Linux"
	LIBS += $(LINUX_GL_LIBS) `pkg-config --static --libs glfw3`

	CXXFLAGS += `pkg-config --cflags glfw3`
	CFLAGS = $(CXXFLAGS)
endif

ifeq ($(UNAME_S), Darwin) #APPLE
	ECHO_MESSAGE = "Mac OS X"
	LIBS += -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
	LIBS += -L/usr/local/lib -L/opt/local/lib -L/opt/homebrew/lib
	#LIBS += -lglfw3
	LIBS += -lglfw

	CXXFLAGS += -I/usr/local/include -I/opt/local/include -I/opt/homebrew/include
	CFLAGS = $(CXXFLAGS)
endif

ifeq ($(OS), Windows_NT)
	ECHO_MESSAGE = "MinGW"
	LIBS += -lglfw3 -lgdi32 -lopengl32 -limm32

	CXXFLAGS += `pkg-config --cflags glfw3`
	CFLAGS = $(CXXFLAGS)
endif

##---------------------------------------------------------------------
## BUILD RULES
##---------------------------------------------------------------------

build/%.o:%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<
build/%.o:%.c
	$(CC) $(CFLAGS) -c -o $@ $<
build/%.o:$(COMMON)/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<
build/%.o:$(COMMON)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<
build/%.o:glad/%.c
	$(CC) $(CFLAGS) -c -o $@ $<
build/%.o:lodepng/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

build/%.o:$(IMGUI_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

build/%.o:$(IMGUI_DIR)/backends/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

all: $(EXE)
	@echo Build complete for $(ECHO_MESSAGE)

$(EXE): $(OBJS)
	$(CXX) -o $@ $^ $(CXXFLAGS) $(LIBS)

XTC_H = $(COMMON)/xtc.h $(COMMON)/xmath.h xtcplat.h xtci.h
XMODEL_H = $(COMMON)/xmodel.h $(XTC_H)
build/main.o: main.cpp app.h $(XTC_H)
build/app.o: app.cpp app.h $(XMODEL_H)
build/conv.o: conv.cpp conv.h $(XMODEL_H)
build/xmodel.o: $(COMMON)/xmodel.cpp $(XMODEL_H) $(COMMON)/chunk.h
build/xanim.o: $(COMMON)/xanim.cpp $(XMODEL_H)
build/xdraw.o: $(COMMON)/xdraw.cpp $(XMODEL_H)
build/chunk.o: $(COMMON)/chunk.c $(COMMON)/chunk.h
build/lua.o: lua.cpp app.h conv.h assimp_rw.h $(XMODEL_H)
build/assimp_rw.o: assimp_rw.cpp assimp_rw.h
build/xtc.o: xtc.cpp $(XTC_H) inc/shader.frag.inc inc/shader.vert.inc inc/skin.vert.inc inc/lit.vert.inc inc/tex.frag.inc

# GLSL sources as C strings, e.g. shader.vert -> inc/shader.vert.inc defining shader_vert_src
inc/%.inc: %
	@mkdir -p inc
	@echo "const char *$(subst .,_,$*)_src =" > $@
	@sed -e 's/\\/\\\\/g' -e 's/"/\\"/g' -e 's/^/"/' -e 's/$$/\\n"/' $< >> $@
	@echo ";" >> $@

clean:
	rm -f $(EXE) $(OBJS)

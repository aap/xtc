/*
 * App glue: init, per-frame hooks into Lua, mouse dragging, helpers.
 * Everything scene-related lives in init.lua / main.fnl.
 */

#include "xtc.h"
#include "xmodel.h"
#include "app.h"
#include "glad/glad.h"
#include "lodepng/lodepng.h"

#include "imgui.h"

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

void
InitGL(void *loadproc)
{
	gladLoadGLLoader((GLADloadproc)loadproc);
}

lua_State *initLua(int argc, char **argv);
lua_State *lua;

bool appShouldQuit;

void
InitApp(int argc, char **argv)
{
	xtcInit();
	lua = initLua(argc, argv);
}

static void
luaCall(const char *code)
{
	if(luaL_dostring(lua, code) != LUA_OK) {
		fprintf(stderr, "error: %s\n", lua_tostring(lua, -1));
		lua_pop(lua, 1);
		exit(1);
	}
}

void
InitScene(void)
{
	if(luaL_dofile(lua, "init.lua") != LUA_OK) {
		fprintf(stderr, "error: %s\n", lua_tostring(lua, -1));
		lua_pop(lua, 1);
		exit(1);
	}
	luaCall("init()\n");
}

void
RenderScene(void)
{
	luaCall("draw()\n");
}

void
GUI(void)
{
	DragStuff();
	// optional
	luaCall("if gui then gui() end\n");
}


/*
 * Helpers
 */

xtcMaterial
DefaultMaterial(void)
{
	xtcMaterial mat;
	mat.colorSelector = vec4(0.0f, 0.0f, 0.0f, 0.0f);
	mat.ambient = vec4(0.2f, 0.2f, 0.2f, 1.0f);
	mat.diffuse = vec4(0.8f, 0.8f, 0.8f, 1.0f);
	mat.specular = vec4(0.0f, 0.0f, 0.0f, 1.0f);
	mat.emissive = vec4(0.0f, 0.0f, 0.0f, 1.0f);
	mat.shininess = 0.0f;
	return mat;
}

// vertex colours only
static xtcMaterial*
axisMaterial(void)
{
	static xtcMaterial mat;
	static bool init;
	if(!init) {
		mat = DefaultMaterial();
		mat.colorSelector = vec4(0.0f, 0.0f, 0.0f, 1.0f);
		mat.ambient = vec4(0.0f, 0.0f, 0.0f, 1.0f);
		mat.diffuse = vec4(0.0f, 0.0f, 0.0f, 1.0f);
		init = true;
	}
	return &mat;
}

void
DrawAxes(float scale = 1.0f)
{
	xtcSetPipeline(defaultPipeline);
	xtcSetTexture(nil);
	xtcSetMaterial(axisMaterial());

	xtcBegin(XTC_LINELIST);
		xtcColor(255, 0, 0, 255);
		xtcVertex3(0.0f, 0.0f, 0.0f);
		xtcVertex3(scale, 0.0f, 0.0f);

		xtcColor(0, 255, 0, 255);
		xtcVertex3(0.0f, 0.0f, 0.0f);
		xtcVertex3(0.0f, scale, 0.0f);

		xtcColor(0, 0, 255, 255);
		xtcVertex3(0.0f, 0.0f, 0.0f);
		xtcVertex3(0.0f, 0.0f, scale);
	xtcEnd();
}

FILE*
efopen(const char *path, const char *mode)
{
	FILE *f = fopen(path, mode);
	if(f == nil) {
		fprintf(stderr, "couldn't open file <%s>\n", path);
		exit(1);
	}
	return f;
}

// GTA cars: hide damaged and low-LOD parts
void
hideCarParts(xNode *node)
{
	if(strstr(node->name, "_dam") ||
	   strstr(node->name, "_vlo"))
		node->hidden = true;
	for(xNode *child = node->child; child; child = child->next)
		hideCarParts(child);
}

// dump the current framebuffer as PNG
int
Screenshot(const char *path)
{
	u32 w = display_w, h = display_h;
	u8 *pixels = (u8*)malloc(w*h*4);
	u8 *row = (u8*)malloc(w*4);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	// GL is bottom-up
	for(u32 y = 0; y < h/2; y++) {
		memcpy(row, pixels + y*w*4, w*4);
		memcpy(pixels + y*w*4, pixels + (h-1-y)*w*4, w*4);
		memcpy(pixels + (h-1-y)*w*4, row, w*4);
	}
	for(u32 i = 0; i < w*h; i++)
		pixels[i*4+3] = 255;
	u32 err = lodepng_encode32_file(path, pixels, w, h);
	free(row);
	free(pixels);
	if(err)
		fprintf(stderr, "screenshot: %s\n", lodepng_error_text(err));
	return err == 0;
}


/*
 * Mouse dragging
 */

int dragging;
bool startDragging;
bool stopDragging;
bool dragCtrl;
bool dragShift;
bool dragAlt;
Vec2 dragStart, dragEnd, dragDelta;

void
DragStuff(void)
{
	ImGuiIO &io = ImGui::GetIO();
	if(stopDragging)
		dragging = 0;
	startDragging = false;
	stopDragging = false;
	if(!dragging) {
		if(!io.WantCaptureMouse && !io.WantCaptureKeyboard) {
			if(ImGui::IsMouseDragging(0, 0.0f))
				dragging = 1;
			else if(ImGui::IsMouseDragging(1, 0.0f))
				dragging = 2;
			else if(ImGui::IsMouseDragging(2, 0.0f))
				dragging = 3;
		}
		if(dragging) {
			dragCtrl = io.KeyCtrl;
			dragShift = io.KeyShift;
			dragAlt = io.KeyAlt;
			dragStart = ImGui::GetMousePos();
			dragEnd = ImGui::GetMousePos();
			startDragging = true;
		}
	} else {
		Vec2 pos = ImGui::GetMousePos();
		dragDelta = pos - dragEnd;
		dragEnd = ImGui::GetMousePos();
		if(!ImGui::IsMouseDragging(dragging-1, 0.0f))
			stopDragging = true;
	}
}

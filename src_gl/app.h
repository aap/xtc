#pragma once

#include "xtc.h"

#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <vector>

extern int display_w, display_h;
extern bool appShouldQuit;

void InitApp(int argc, char **argv);
void InitGL(void* loadproc);
void InitScene(void);
void RenderScene(void);
void GUI(void);
void DragStuff(void);

FILE *efopen(const char *path, const char *mode);
int Screenshot(const char *path);

extern int dragging;
extern bool startDragging, stopDragging;
extern bool dragCtrl, dragShift, dragAlt;
extern Vec2 dragStart, dragEnd, dragDelta;

template <typename T> T min(T a, T b) { return a < b ? a : b; }
template <typename T> T max(T a, T b) { return a > b ? a : b; }
template <typename T> T clamp(T a, T l, T h) { return a > h ? h : a < l ? l : a; }
template <typename T> T sq(T a) { return a*a; }

#define IM_VEC2_CLASS_EXTRA                                                     \
        constexpr ImVec2(const Vec2 &f) : x(f.x), y(f.y) {}                   \
        operator Vec2() const { return vec2(x, y); } 

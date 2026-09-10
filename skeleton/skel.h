#ifndef SKEL_H
#define SKEL_H

/*
 * The part of a PS2 program that is the same in every one of them:
 * the graphics context and its two buffers, the VIF list the library
 * draws into, the pad, and the frame loop.  librw has a skeleton like
 * this and for the same reason -- a demo should be about what it draws.
 *
 *	skelInit(640, 448);
 *	for(;;) {
 *		skelBeginFrame();
 *		... draw ...
 *		skelEndFrame();
 *	}
 *
 * or, when the program has nothing to say around the loop,
 *
 *	skelInit(640, 448);
 *	skelRun(drawFrame);	// never returns
 *
 * skelInit brings up SIF RPC, the pad (joy.h), MDMA, the GS and xtc,
 * and clears both buffers.  skelBeginFrame reads the pad, resets the
 * list, points xtc at this field's draw buffer, clears it and puts the
 * render state back to the baseline (see skelDefaultState), so a draw
 * function only sets what it wants and cleans up nothing.
 * skelEndFrame closes the list, kicks it, waits for vsync and flips.
 */

#include "xtci.h"

extern int skelWidth, skelHeight;	/* the draw buffer, in pixels */
extern xtcgBuffers skelBuffers;

void skelInit(int width, int height);
void skelBeginFrame(void);
void skelEndFrame(void);
void skelRun(void (*frame)(void));

/* depth test on, blend/fog/texture/clipping off, no texture, linear
 * filter, RGB modulate, world matrix identity.  skelBeginFrame calls it */
void skelDefaultState(void);

/* the colour skelBeginFrame clears to; 0-255, mid grey by default */
void skelClearColor(int r, int g, int b);

/* frames since skelInit */
extern int skelFrameCount;

/* the last frame, in COP0 Count ticks: the EE building the list between
 * skelBeginFrame and the kick, the DMA/VIF/VU/GS running it (the kick
 * to sceGsSyncPath returning), and the wait for vsync after that.
 * cheap enough to be always on */
extern unsigned int skelTimeCpu, skelTimeGs, skelTimeVsync;
unsigned int skelCount(void);

#endif

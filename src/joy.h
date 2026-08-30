#ifndef JOY_H
#define JOY_H

/*
 * Comfy interface to the PS2 pad, lifted from librw's ps2 skeleton.
 *
 * joyInit() loads the IOP modules and opens the pad (sceSifInitRpc
 * must have happened); joyUpdate() once per frame refreshes joy.
 *
 * Two builds, two module sources: with JOY_ROMPAD the modules come out
 * of the boot ROM (rom0:XSIO2MAN + rom0:XPADMAN) and the program must
 * link libopad.a -- the last libpad that accepts the ROM's padman 3.6.
 * Without it sio2man.irx and padman.irx are loaded from host0 and the
 * current libpad.a is the one to link.
 */

enum {
	JOY_L2       = 0x0001,
	JOY_R2       = 0x0002,
	JOY_L1       = 0x0004,
	JOY_R1       = 0x0008,
	JOY_TRIANGLE = 0x0010,
	JOY_CIRCLE   = 0x0020,
	JOY_CROSS    = 0x0040,
	JOY_SQUARE   = 0x0080,
	JOY_SELECT   = 0x0100,
	JOY_L3       = 0x0200,
	JOY_R3       = 0x0400,
	JOY_START    = 0x0800,
	JOY_UP       = 0x1000,
	JOY_RIGHT    = 0x2000,
	JOY_DOWN     = 0x4000,
	JOY_LEFT     = 0x8000
};

typedef struct Joypad Joypad;
struct Joypad
{
	int connected;
	unsigned short btns;	/* held right now */
	unsigned short press;	/* went down this frame */
	unsigned short release;	/* went up this frame */
	/* sticks, -1..1 with deadzone applied; down/right positive */
	float lx, ly;
	float rx, ry;
};

extern Joypad joy;	/* the pad in port 0 */

int joyInit(void);
void joyUpdate(void);

#endif

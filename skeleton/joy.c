#include <stdio.h>
#include <string.h>

#include <eekernel.h>
#include <sifrpc.h>
#include <sifdev.h>
#ifdef JOY_ROMPAD
#include <libopad.h>
#else
#include <libpad.h>
#endif

#include "joy.h"

Joypad joy;

static int gotpad;
static int modeset;
static u_long128 padDmaBuf[scePadDmaBufferMax] __attribute__((aligned(64)));

#define EPSILON 0.2f

static float
analog(int x)
{
	float f = (x/255.0f - 0.5f)*2.0f;
	if(f > EPSILON) return (f-EPSILON)/(1.0f-EPSILON);
	if(f < -EPSILON) return (f+EPSILON)/(1.0f-EPSILON);
	return 0.0f;
}

// Switch the pad into analog mode ourselves so nobody has to reach for
// the ANALOG button.  scePadSetMainMode is an RPC to padman that gets
// refused (returns 0) until the pad state is Stable/FindCTP1, so it can't
// go in joyInit right after PortOpen - the SDK samples spin on it with a
// vsync per retry.  We're called every frame anyway, so poll the state
// instead: on the first stable frame ask once - offs 1 picks the analog
// entry of the mode table, lock 0 keeps the ANALOG button alive.  One
// shot per connection: a refusal when stable means a pad with no analog
// mode, and the ExecCmd/Stable dance during the switch itself must not
// retrigger us.  Disconnect re-arms, so a re-plugged pad is switched too.
static void
setAnalogMode(void)
{
	int state;

	state = scePadGetState(0, 0);
	if(state == scePadStateDiscon)
		modeset = 0;
	if(!modeset && (state == scePadStateStable || state == scePadStateFindCTP1)){
		scePadSetMainMode(0, 0, 1, 0);
		modeset = 1;
	}
}

void
joyUpdate(void)
{
	unsigned char rdata[32];
	unsigned short btns;

	if(gotpad)
		setAnalogMode();

	btns = 0;
	joy.rx = joy.ry = 0.0f;
	joy.lx = joy.ly = 0.0f;
	if(gotpad && scePadRead(0, 0, rdata) > 0){
		btns = 0xFFFF ^ ((rdata[2] << 8) | rdata[3]);
		if(rdata[1] == 0x73){
			joy.rx = analog(rdata[4]);
			joy.ry = analog(rdata[5]);
			joy.lx = analog(rdata[6]);
			joy.ly = analog(rdata[7]);
		}
	}
	joy.press = btns & ~joy.btns;
	joy.release = joy.btns & ~btns;
	joy.btns = btns;
	joy.connected = gotpad;
}

/*
 * IOP modules
 */

#ifdef JOY_ROMPAD

// Load an IOP module out of the boot ROM.  sceSifLoadModule can't do
// this: libkernl asks the loadfile server for its version over a one-off
// fno 0xff RPC and won't go on unless the answer matches the SDK's own
// tag, and the ROM's LOADFILE doesn't implement fno 0xff at all - it
// replies 0, so every sceSifLoadModule against it returns
// -SCE_EVERSIONMISS.  Same server, same wire protocol, no handshake:
// fno 0, arg length in word 0, path at byte 8.
#define LOADFILE_RPC_ID 0x80000006

static sceSifClientData loadfileCd __attribute__((aligned(64)));
static unsigned char loadfileBuf[512] __attribute__((aligned(64)));
static int boundLoadfile;

static void
rpcDelay(int n)
{
	volatile int i;

	for(i = 0; i < n; i++);
}

static int
romLoadModule(const char *module)
{
	if(!boundLoadfile){
		for(;;){
			if(sceSifBindRpc(&loadfileCd, LOADFILE_RPC_ID, 0) < 0)
				return -1;
			if(loadfileCd.serve)
				break;
			rpcDelay(0x10000);
		}
		boundLoadfile = 1;
	}

	memset(loadfileBuf, 0, sizeof(loadfileBuf));
	strncpy((char*)loadfileBuf+8, module, 252);
	if(sceSifCallRpc(&loadfileCd, 0, 0, loadfileBuf, 512, loadfileBuf, 8, NULL, NULL) < 0)
		return -2;
	// the module id, or a negative loadcore error
	// (-200 not found, MODULE_NO_RESIDENT_END if it was already there)
	return *(int*)loadfileBuf;
}

static void
loadModules(void)
{
	int r;

	// The boot ROM's own pair - nothing shipped next to the ELF and no
	// IOP reboot.  XPADMAN imports sio2man 1.02, which only XSIO2MAN
	// provides, so rom0:SIO2MAN won't do.  XPADMAN is padman 3.6 and
	// libopad (the 2.0 libpad) is the last one that talks to a 3.x
	// padman; whether we really got a pad is joyInit's call.
	gotpad = 1;
	r = romLoadModule("rom0:XSIO2MAN");
	if(r < 0)
		printf("rom0:XSIO2MAN -> %d\n", r);
	r = romLoadModule("rom0:XPADMAN");
	if(r < 0)
		printf("rom0:XPADMAN -> %d\n", r);
}

#else

static void
loadModules(void)
{
	int i;

	gotpad = 1;
	for(i = 0; i < 10; i++)
		if(sceSifLoadModule("host0:/usr/local/sce/iop/modules/sio2man.irx", 0, NULL) >= 0)
			goto sio2ok;
	printf("can't load module sio2man\n");
	gotpad = 0;
	return;
sio2ok:
	for(i = 0; i < 10; i++)
		if(sceSifLoadModule("host0:/usr/local/sce/iop/modules/padman.irx", 0, NULL) >= 0)
			return;
	printf("can't load module padman\n");
	gotpad = 0;
}

#endif

int
joyInit(void)
{
	loadModules();
	if(!gotpad)
		return 0;
	if(scePadInit(0) == 0 || scePadPortOpen(0, 0, padDmaBuf) == 0){
		printf("can't init pad\n");
		gotpad = 0;
	}
	return gotpad;
}

#ifndef KERNEL_SHIM_H_
#define KERNEL_SHIM_H_

#include "types.h"

#ifndef _PS2SDK
#include <eekernel.h>
uint64 GsPutIMR(uint64 imr);
uint64 GsPutIMR(uint64 imr);
void   SetGsCrt(int16 interlace, int16 pal_ntsc, int16 field);
#define ShimInitRpc sceSifInitRpc
#else
#include <kernel.h>
#define ShimInitRpc SifInitRpc
#endif

#endif // KERNEL_SHIM_H_

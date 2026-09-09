/* PS2: what common/xtc.h needs before anything else.  mdma brings the
 * fixed-width types (and uint128) for this toolchain. */

#ifndef XTCPLAT_H
#define XTCPLAT_H

#include "mdma.h"
#include <stddef.h>

typedef  int64_t  int64;
typedef uint64_t uint64;
typedef  int32_t  int32;
typedef uint32_t uint32;
typedef  int16_t  int16;
typedef uint16_t uint16;
typedef  int8_t   int8;
typedef uint8_t  uint8;
typedef uintptr_t uintptr;
typedef uint128_t uint128;

#define nil NULL

#endif

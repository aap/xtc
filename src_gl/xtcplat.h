/* OpenGL sketch: what common/xtc.h needs before anything else. */

#ifndef XTCPLAT_H
#define XTCPLAT_H

#include <stdint.h>
#include <stddef.h>

typedef uint64_t uint64;
typedef int64_t int64;
typedef uint32_t uint32;
typedef uint16_t uint16;
typedef uint8_t uint8;
typedef int32_t int32;
typedef int16_t int16;
typedef int8_t int8;
typedef uintptr_t uintptr;
/* the short names the sketch grew up with */
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;
typedef int32_t i32;
typedef int16_t i16;
typedef int8_t i8;

#ifdef __cplusplus
#define nil nullptr
#else
#define nil ((void*)0)
#endif

#endif

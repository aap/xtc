#ifndef TYPES_H_
#define TYPES_H_

typedef long long	   int64;
typedef unsigned long long uint64;
typedef int		   int32;
typedef unsigned int	   uint32;
typedef short		   int16;
typedef unsigned short	   uint16;
typedef signed char	   int8;
typedef unsigned char	   uint8;
typedef uint32		   uintptr;

typedef unsigned int uint128 __attribute__((mode(TI)));
static inline uint128
MAKE128(uint64 msb, uint64 lsb)
{
	uint128 data;
#ifndef _PS2SDK
	__asm__("pcpyld %0, %1, %2" : "=r"(data) : "r"((uint64)(msb)), "r"((uint64)(lsb)));
#else
	data = (uint128)lsb | ((uint128)msb << 64);
#endif
	return data;
}
#define UINT64(HIGH, LOW) (((uint64)(uint32)(HIGH)) << 32 | ((uint64)(uint32)(LOW)))
#define MAKEQ(W3, W2, W1, W0) MAKE128(UINT64(W3, W2), UINT64(W1, W0))

#endif // TYPES_H_

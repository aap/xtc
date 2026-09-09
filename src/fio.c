#include <stdio.h>
#include <string.h>

#include <eekernel.h>
#include <sifrpc.h>
#include <sifcmd.h>
#include <sifdev.h>

#include "fio.h"

#define FIO_RPC_ID 0x80000001

/* the wire fnos; the same numbers every FILEIO speaks, it's the
 * payload shapes that changed over the versions */
enum {
	FIO_OPEN	= 0,
	FIO_CLOSE	= 1,
	FIO_READ	= 2,
	FIO_WRITE	= 3,
	FIO_LSEEK	= 4,
};

#define FIO_PATH_MAX 256

/* The server DMAs a read's unaligned head and tail here, with the
 * aligned middle going straight into the caller's buffer.  16-byte
 * slots: the ROM module is the pre-1500 vintage -- the updated FILEIOs
 * shipped with games grew these to 64 and broke compatibility. */
struct ReadData {
	unsigned int size1;
	unsigned int size2;
	void *dest1;
	void *dest2;
	unsigned char buf1[16];
	unsigned char buf2[16];
};

union Arg {
	struct {
		int flags;
		char path[FIO_PATH_MAX];
	} open;
	struct {
		int fd;
	} close;
	struct {
		int fd;
		void *buf;
		int n;
		struct ReadData *rdata;
	} read;
	struct {
		int fd;
		const void *buf;
		unsigned int n;
		unsigned int mis;
		unsigned char misbuf[16];
	} write;
	struct {
		int fd;
		int offset;
		int whence;
	} lseek;
};

/* The RPC replies land in these by SIF DMA, which writes in 16-byte
 * units regardless of rsize -- and a dirty cacheline shared with a
 * neighbour can be written back over freshly-DMA'd data.  So every
 * receive buffer gets a full 64-byte line to itself; a bare 4-byte
 * result once let the reply DMA stomp the statics linked after it
 * (joy.c's mode flag among them). */
static sceSifClientData fioCd __attribute__((aligned(64)));
static union Arg arg __attribute__((aligned(64)));
static union { int val; unsigned char line[64]; } result __attribute__((aligned(64)));
static union { struct ReadData rd; unsigned char line[64]; } rdata __attribute__((aligned(64)));
static int bound;

/* the RPC replies land by DMA; read them past the cache */
#define UNCACHED(p) ((void*)(0x20000000 | (unsigned int)(p)))

static int
bind(void)
{
	volatile int i;

	if(bound)
		return 0;
	for(;;){
		if(sceSifBindRpc(&fioCd, FIO_RPC_ID, 0) < 0)
			return -1;
		if(fioCd.serve)
			break;
		for(i = 0; i < 0x10000; i++);
	}
	bound = 1;
	return 0;
}

int
fioOpen(const char *path, int flags)
{
	if(bind() < 0)
		return -1;
	arg.open.flags = flags;
	strncpy(arg.open.path, path, FIO_PATH_MAX);
	arg.open.path[FIO_PATH_MAX-1] = 0;
	if(sceSifCallRpc(&fioCd, FIO_OPEN, 0, &arg, sizeof(arg.open), &result, sizeof(result), NULL, NULL) < 0)
		return -1;
	return *(int*)UNCACHED(&result.val);
}

int
fioClose(int fd)
{
	if(bind() < 0)
		return -1;
	arg.close.fd = fd;
	if(sceSifCallRpc(&fioCd, FIO_CLOSE, 0, &arg, sizeof(arg.close), &result, sizeof(result), NULL, NULL) < 0)
		return -1;
	return *(int*)UNCACHED(&result.val);
}

int
fioRead(int fd, void *buf, int n)
{
	struct ReadData *rd;

	if(bind() < 0)
		return -1;
	arg.read.fd = fd;
	arg.read.buf = buf;
	arg.read.n = n;
	arg.read.rdata = &rdata.rd;
	/* writeback-invalidate: the middle of the read is DMA'd into buf
	 * behind the cache's back */
	sceSifWriteBackDCache(buf, n);
	sceSifWriteBackDCache(&rdata, sizeof(rdata));
	if(sceSifCallRpc(&fioCd, FIO_READ, 0, &arg, sizeof(arg.read), &result, sizeof(result), NULL, NULL) < 0)
		return -1;
	rd = (struct ReadData*)UNCACHED(&rdata);
	if(rd->size1)
		memcpy(rd->dest1, rd->buf1, rd->size1);
	if(rd->size2)
		memcpy(rd->dest2, rd->buf2, rd->size2);
	return *(int*)UNCACHED(&result.val);
}

int
fioWrite(int fd, const void *buf, int n)
{
	unsigned int mis;

	if(bind() < 0)
		return -1;
	/* unaligned head goes inline in the request, the rest is DMA'd
	 * from the buffer itself */
	mis = (unsigned int)buf & 0xF;
	if(mis)
		mis = 16 - mis;
	if(mis > (unsigned int)n)
		mis = n;
	arg.write.fd = fd;
	arg.write.buf = buf;
	arg.write.n = n;
	arg.write.mis = mis;
	if(mis)
		memcpy(arg.write.misbuf, buf, mis);
	sceSifWriteBackDCache(buf, n);
	if(sceSifCallRpc(&fioCd, FIO_WRITE, 0, &arg, sizeof(arg.write), &result, sizeof(result), NULL, NULL) < 0)
		return -1;
	return *(int*)UNCACHED(&result.val);
}

int
fioLseek(int fd, int offset, int whence)
{
	if(bind() < 0)
		return -1;
	arg.lseek.fd = fd;
	arg.lseek.offset = offset;
	arg.lseek.whence = whence;
	if(sceSifCallRpc(&fioCd, FIO_LSEEK, 0, &arg, sizeof(arg.lseek), &result, sizeof(result), NULL, NULL) < 0)
		return -1;
	return *(int*)UNCACHED(&result.val);
}

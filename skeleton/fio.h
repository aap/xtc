#ifndef FIO_H
#define FIO_H

/*
 * EE client for the boot ROM's FILEIO module -- the 99/11/15 "Multi
 * Threaded Fileio module" (SDK 1.3 vintage) serving RPC 0x80000001,
 * which is what runs when nothing reboots the IOP: PCSX2 booting an
 * ELF, a retail machine, a CD boot without an IOP reboot.
 *
 * libkernl's sceOpen cannot talk to it: from 2.0 on it demands a
 * version handshake (fno 0xff) the ROM module doesn't implement, and
 * the later wire protocols are incompatible anyway (2200 moved result
 * delivery to SIF cmds).  This speaks the old protocol: synchronous
 * RPCs, the result in the reply word, a read's unaligned edges
 * delivered through a side structure with 16-byte slots.
 *
 * Use sceOpen instead on a TOOL or after an IOP reboot with a newer
 * FILEIO -- the two protocols don't mix.
 *
 * PCSX2 note: relative paths want the ./ spelled out -- "host:./file"
 * -- and resolve relative to the ELF's directory.
 *
 * Flags and whence values are sifdev.h's SCE_RDONLY / SCE_SEEK_SET
 * families; the ROM module uses the same numbering.
 */

int fioOpen(const char *path, int flags);
int fioClose(int fd);
int fioRead(int fd, void *buf, int n);
int fioWrite(int fd, const void *buf, int n);
int fioLseek(int fd, int offset, int whence);

#endif

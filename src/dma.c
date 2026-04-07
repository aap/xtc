#include "types.h"

#include <stdio.h>

static void
indent(int n)
{
	while(n--)
		printf("    ");
}

static void
dumpData(uint32 *data, int qwc, int depth)
{
	while(qwc--){
		float *fdata = (float*)data;
		indent(depth+1);
		printf("%08X: ", (uint32)data);
		printf("%08X %08X %08X %08X\t", data[0], data[1], data[2], data[3]);
		printf("%.3f %.3f %.3f %.3f\n", fdata[0], fdata[1], fdata[2], fdata[3]);
		data += 4;
	}
}

void
dumpDma(uint32 *packet, int data)
{
	uint32 *addr, *next;
	uint32 *stack[2];
	uint32	qwc;
	int sp = 0;
	int end = 0;
	printf("packet start: %p\n", packet);
	while(!end){
		qwc = packet[0]&0xFFFF;
		addr = (uint32 *)packet[1];
		indent(sp);
		printf("%08X: %08X %08X %08X %08X\n", (uint32)packet, packet[0], packet[1],
		    packet[2], packet[3]);
		switch((packet[0]>>28) & 7){
		case 0:
			indent(sp);
			printf("refe %p, %X\n", addr, qwc);
			if(data) dumpData(addr, qwc, sp);
			end = 1;
			next = NULL;
			break;
		case 1:
			indent(sp);
			printf("cnt %p, %X\n", addr, qwc);
			if(data) dumpData(packet+4, qwc, sp);
			next = packet + (1+qwc)*4;
			break;
		case 2:
			indent(sp);
			printf("next %p, %X\n", addr, qwc);
			if(data) dumpData(packet+4, qwc, sp);
			next = addr;
			break;
		case 3:
			indent(sp);
			printf("ref %p, %X\n", addr, qwc);
			if(data) dumpData(addr, qwc, sp);
			next = packet + 4;
			break;
		case 4:
			indent(sp);
			printf("refs %p, %X\n", addr, qwc);
			if(data) dumpData(addr, qwc, sp);
			next = packet + 4;
			break;
		case 5:
			indent(sp);
			printf("call %p, %X\n", addr, qwc);
			if(data) dumpData(packet+4, qwc, sp);
			stack[sp++] = packet + (1+qwc)*4;
			if(sp > 2){
				printf("DMA stack overflow\n\n\n");
				return;
			}
			next = addr;
			break;
		case 6:
			indent(sp);
			printf("ret %p, %X\n", addr, qwc);
			if(data) dumpData(packet+4, qwc, sp);
			next = stack[--sp];
			if(sp < 0){
				printf("DMA stack underflow\n\n\n");
				return;
			}
			break;
		case 7:
			indent(sp);
			printf("end %p, %X\n", addr, qwc);
			if(data) dumpData(packet+4, qwc, sp);
			end = 1;
			next = NULL;
			break;
		}
		packet = next;
	}
	printf("packet end\n\n\n");
}


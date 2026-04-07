#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

#include "chunk.h"

#define nil NULL
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;
typedef int32_t i32;
typedef int16_t i16;
typedef int8_t i8;
typedef uintptr_t uintptr;

typedef struct Block Block;
typedef struct Pointer Pointer;
typedef struct ChunkData ChunkData;

typedef struct sChunkHeader sChunkHeader;
struct sChunkHeader
{
	u32 ident;
	u32 shrink;
	u32 fileEnd;
	u32 dataEnd;
	u32 relocTab;
	u32 numRelocs;
	u32 globalTab;
	u16 numClasses;
	u16 numFuncs;
};

struct Block
{
	void *base, *end;
	size_t size;
	int align;
	size_t offset;	// in file
};

struct Pointer
{
	void **ptr;
	void *savedValue;
	int block;
};

struct ChunkData
{
	int numBlocks;
	Block *blocks;
	int numPointers;
	Pointer *pointers;
	int numRelocations;
	u32 *relocations;
};

ChunkData*
makeChunkData(void)
{
	ChunkData *chk;
	chk = malloc(sizeof(ChunkData));
	chk->numBlocks = 0;
	chk->blocks = nil;
	chk->numPointers = 0;
	chk->pointers = nil;
	chk->numRelocations = 0;
	chk->relocations = nil;
	return chk;
}

void
freeChunkData(ChunkData *chk)
{
	free(chk->blocks);
	free(chk->pointers);
	free(chk->relocations);
	free(chk);
}

static Block*
findBlock(ChunkData *chk, void *base)
{
	int i;
	for(i = 0; i < chk->numBlocks; i++)
		if(chk->blocks[i].base == base)
			return &chk->blocks[i];
	return nil;
}

static Block*
addBlock(ChunkData *chk, void *base, size_t size, int align)
{
	Block *b;
	chk->numBlocks++;
	chk->blocks = realloc(chk->blocks, chk->numBlocks*sizeof(Block));
	assert(chk->blocks);
	b = &chk->blocks[chk->numBlocks-1];
//printf("adding block %p\n", base);
	b->base = base;
	b->end = (u8*)base + size;
	b->size = size;
	b->align = align;
	return b;
}

int
registerBlock(ChunkData *chk, void *base, size_t size, int align)
{
	if(findBlock(chk, base) == nil){
		addBlock(chk, base, size, align);
		return 0;
	}
	return 1;
}

static Pointer*
findPointer(ChunkData *chk, void **ptr)
{
	int i;
	for(i = 0; i < chk->numPointers; i++)
		if(chk->pointers[i].ptr == ptr)
			return &chk->pointers[i];
	return nil;
}

static Pointer*
addPointer(ChunkData *chk, void **ptr)
{
	Pointer *p;
	Block *b;

	chk->numPointers++;
	chk->pointers = realloc(chk->pointers, chk->numPointers*sizeof(Pointer));
	assert(chk->pointers);
	p = &chk->pointers[chk->numPointers-1];
//printf("adding ptr %p\n", ptr);
	p->ptr = ptr;
	p->savedValue = *ptr;
	b = &chk->blocks[chk->numBlocks-1];
	assert((void*)ptr >= b->base);
	assert((void*)ptr < b->end);
	p->block = chk->numBlocks-1;
	return p;
}

int
registerPointer(ChunkData *chk, void *ptr)
{
	if(findPointer(chk, (void**)ptr) == nil){
		addPointer(chk, (void**)ptr);
		return 0;
	}
	return 1;
}

static Block*
findBlockPointingTo(ChunkData *chk, void *ptr)
{
	int i;
	Block *block;
	for(i = 0; i < chk->numBlocks; i++){
		block = &chk->blocks[i];
		if(ptr >= block->base && ptr < block->end)
			return block;
	}
	return nil;
}

static void
fixPointers(ChunkData *chk)
{
	int i;

	chk->relocations = malloc(chk->numPointers*sizeof(u32));
	chk->numRelocations = 0;

	for(i = 0; i < chk->numPointers; i++){
//		chk->pointers[i].savedValue = *chk->pointers[i].ptr;

		Block *block = findBlockPointingTo(chk, *chk->pointers[i].ptr);
		if(block == nil)
			continue;
		*chk->pointers[i].ptr = ((u8*)*chk->pointers[i].ptr - (uintptr)block->base + block->offset);
		block = &chk->blocks[chk->pointers[i].block];
		chk->relocations[chk->numRelocations++] = (u32)((uintptr)chk->pointers[i].ptr -
			 (uintptr)block->base + block->offset);
	}
}

static void
restorePointers(ChunkData *chk)
{
	int i;
	for(i = 0; i < chk->numPointers; i++)
		*chk->pointers[i].ptr = chk->pointers[i].savedValue;
}

void
writeChunk(ChunkData *chk, FILE *f)
{
	int i;
	size_t totalSize, off;
	u8 *buffer;
	sChunkHeader *header;

	totalSize = sizeof(sChunkHeader);
	for(i = 0; i < chk->numBlocks; i++){
		Block *b = &chk->blocks[i];

		totalSize = (totalSize + b->align-1) & ~(b->align-1);
		b->offset = totalSize;
//printf("block at %lx %p\n", b->offset, b->base);
		totalSize += b->size;
	}

	fixPointers(chk);

	buffer = malloc(totalSize);
	memset(buffer, 0xAA, totalSize);
	header = (sChunkHeader*)buffer;
	memset(header, 0, sizeof(sChunkHeader));
	off = sizeof(sChunkHeader);
	for(i = 0; i < chk->numBlocks; i++){
		Block *b = &chk->blocks[i];

		off = (off + b->align-1) & ~(b->align-1);
		memcpy(&buffer[off], b->base, b->size);
		off += b->size;
	}

	restorePointers(chk);

	header->ident = 'ABCD';
	header->shrink = 0;
	header->fileEnd = totalSize + sizeof(u32)*chk->numRelocations;
	header->dataEnd = totalSize;
	header->relocTab = totalSize;
	header->numRelocs = chk->numRelocations;

	fwrite(buffer, 1, totalSize, f);
	fwrite(chk->relocations, sizeof(u32), chk->numRelocations, f);
	free(buffer);
}

void*
loadChunk(FILE *f)
{
	sChunkHeader header;
	u8 *data;
	u32 *reloc;

	fread(&header, 1, sizeof(header), f);
//printf("%08X %08X %08X %08X\n", header.ident, header.shrink, header.fileEnd, header.dataEnd);
//printf("%08X %08X %08X %04X %04X\n", header.relocTab, header.numRelocs, header.globalTab, header.numClasses, header.numFuncs);
	data = (u8*)malloc(header.dataEnd - sizeof(header));
	reloc = (u32*)malloc(header.numRelocs*sizeof(u32));
	fread(data, 1, header.dataEnd - sizeof(header), f);
	fread(reloc, 1, header.numRelocs*sizeof(u32), f);

	uintptr off = (uintptr)data - sizeof(header);
	for(int i = 0; i < header.numRelocs; i++)
		*(uintptr*)&data[reloc[i]-sizeof(header)] += off;
//	for(int i = 0; i < header.numRelocs; i++)
//		printf("%X %p\n", reloc[i], *(void**)&data[reloc[i]-sizeof(header)]);
	free(reloc);

	return data;
}

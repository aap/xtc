#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "xtcplat.h"	// the integer types of the platform at hand
#include "chunk.h"

#ifndef nil
#define nil NULL
#endif
typedef uint32 u32;
typedef uint16 u16;
typedef uint8 u8;
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
	u32 globalTab;	// {u32 location; char cls[]; char name[];} word padded
	u32 globalEnd;
};

#define CHUNK_IDENT 0x41424344	// 'ABCD'

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

typedef struct Global Global;
struct Global
{
	void **ptr;
	int block;
	const char *cls;
	const char *name;
};

struct ChunkData
{
	int numBlocks;
	Block *blocks;
	int numPointers;
	Pointer *pointers;
	int numRelocations;
	u32 *relocations;
	int numGlobals;
	Global *globals;
};

ChunkData*
makeChunkData(void)
{
	ChunkData *chk;
	chk = (ChunkData*)malloc(sizeof(ChunkData));
	chk->numBlocks = 0;
	chk->blocks = nil;
	chk->numPointers = 0;
	chk->pointers = nil;
	chk->numRelocations = 0;
	chk->relocations = nil;
	chk->numGlobals = 0;
	chk->globals = nil;
	return chk;
}

void
freeChunkData(ChunkData *chk)
{
	free(chk->blocks);
	free(chk->pointers);
	free(chk->relocations);
	free(chk->globals);
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
	chk->blocks = (Block*)realloc(chk->blocks, chk->numBlocks*sizeof(Block));
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
	chk->pointers = (Pointer*)realloc(chk->pointers, chk->numPointers*sizeof(Pointer));
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

// a pointer to something outside the chunk, written as 0 and resolved
// by class and name at load time.  like registerPointer, the field has
// to be in the block registered last.
void
registerGlobal(ChunkData *chk, void *ptr, const char *cls, const char *name)
{
	Global *g;
	Block *b;

	chk->numGlobals++;
	chk->globals = (Global*)realloc(chk->globals, chk->numGlobals*sizeof(Global));
	assert(chk->globals);
	g = &chk->globals[chk->numGlobals-1];
	g->ptr = (void**)ptr;
	b = &chk->blocks[chk->numBlocks-1];
	assert((void*)ptr >= b->base);
	assert((void*)ptr < b->end);
	g->block = chk->numBlocks-1;
	g->cls = cls;
	g->name = name;
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

	chk->relocations = (u32*)malloc(chk->numPointers*sizeof(u32));
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
	size_t totalSize, off, globalSize;
	u8 *buffer;
	sChunkHeader *header;
	u32 fileoff;

	totalSize = sizeof(sChunkHeader);
	for(i = 0; i < chk->numBlocks; i++){
		Block *b = &chk->blocks[i];

		totalSize = (totalSize + b->align-1) & ~(b->align-1);
		b->offset = totalSize;
//printf("block at %lx %p\n", b->offset, b->base);
		totalSize += b->size;
	}

	fixPointers(chk);

	buffer = (u8*)malloc(totalSize);
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

	// the globals' fields hold nothing the file can use
	globalSize = 0;
	for(i = 0; i < chk->numGlobals; i++){
		Global *g = &chk->globals[i];
		Block *b = &chk->blocks[g->block];
		fileoff = (u32)((uintptr)g->ptr - (uintptr)b->base + b->offset);
		memset(&buffer[fileoff], 0, sizeof(void*));
		globalSize += (4 + strlen(g->cls)+1 + strlen(g->name)+1 + 3) & ~3;
	}

	header->ident = CHUNK_IDENT;
	header->shrink = 0;
	header->dataEnd = totalSize;
	header->relocTab = totalSize;
	header->numRelocs = chk->numRelocations;
	header->globalTab = totalSize + sizeof(u32)*chk->numRelocations;
	header->globalEnd = header->globalTab + globalSize;
	header->fileEnd = header->globalEnd;

	fwrite(buffer, 1, totalSize, f);
	fwrite(chk->relocations, sizeof(u32), chk->numRelocations, f);
	for(i = 0; i < chk->numGlobals; i++){
		static const u8 pad[4] = { 0, 0, 0, 0 };
		Global *g = &chk->globals[i];
		Block *b = &chk->blocks[g->block];
		size_t n;
		fileoff = (u32)((uintptr)g->ptr - (uintptr)b->base + b->offset);
		fwrite(&fileoff, sizeof(u32), 1, f);
		fwrite(g->cls, 1, strlen(g->cls)+1, f);
		fwrite(g->name, 1, strlen(g->name)+1, f);
		n = (4 + strlen(g->cls)+1 + strlen(g->name)+1);
		fwrite(pad, 1, ((n + 3) & ~3) - n, f);
	}
	free(buffer);
}

int
isChunk(const uint8 *data, uint32 size)
{
	return size >= sizeof(sChunkHeader) &&
		((const sChunkHeader*)data)->ident == CHUNK_IDENT;
}

void*
loadChunkMem(const uint8 *file, uint32 size, ChunkResolver resolve)
{
	const sChunkHeader *header = (const sChunkHeader*)file;
	u8 *data;
	const u32 *reloc;
	const u8 *p, *end;
	u32 dataSize, loc;
	const char *cls, *name;
	uintptr off;
	int i;

	if(!isChunk(file, size) || header->fileEnd > size){
		printf("loadChunk: not a chunk\n");
		return nil;
	}

	// the data has DMA chains in it, which want qword alignment; the
	// blocks are laid out for that relative to the start of the file
	dataSize = header->dataEnd - sizeof(sChunkHeader);
	data = (u8*)malloc(dataSize + 16);
	data = (u8*)(((uintptr)data + 15) & ~15);
	memcpy(data, file + sizeof(sChunkHeader), dataSize);

	// offsets in the file become addresses
	off = (uintptr)data - sizeof(sChunkHeader);
	reloc = (const u32*)(file + header->relocTab);
	for(i = 0; i < (int)header->numRelocs; i++)
		*(uintptr*)&data[reloc[i]-sizeof(sChunkHeader)] += off;

	// and the globals get looked up
	p = file + header->globalTab;
	end = file + header->globalEnd;
	while(p + 4 <= end){
		memcpy(&loc, p, 4);
		cls = (const char*)p + 4;
		name = cls + strlen(cls) + 1;
		p = (const u8*)name + strlen(name) + 1;
		p = file + (((p - file) + 3) & ~3);
		*(void**)&data[loc-sizeof(sChunkHeader)] = resolve ? resolve(cls, name) : nil;
		if(resolve == nil)
			printf("loadChunk: no resolver for %s %s\n", cls, name);
	}

	return data;
}

void*
loadChunk(FILE *f, ChunkResolver resolve)
{
	u8 *file;
	long size;
	void *data;

	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fseek(f, 0, SEEK_SET);
	file = (u8*)malloc(size);
	if(fread(file, 1, size, f) != (size_t)size){
		free(file);
		return nil;
	}
	data = loadChunkMem(file, (u32)size, resolve);
	free(file);
	return data;
}

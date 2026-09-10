
/* wants the platform's integer types (xtcplat.h) included first;
 * the SCE toolchain has no stdint.h */
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Chunks: a memory image of linked structures, loaded with one read
 * and a pass over the fixup table.  Pointers to things outside the
 * chunk (a pipeline, a texture) are globals: {location, class, name}
 * entries the loader hands to a resolver.  The same file layout comes
 * out of the assembler, see tools/chunk.inc.
 */

typedef struct ChunkData ChunkData;
typedef void *(*ChunkResolver)(const char *cls, const char *name);

ChunkData *makeChunkData(void);
void freeChunkData(ChunkData *chk);
int registerBlock(ChunkData *chk, void *base, size_t size, int align);
int registerPointer(ChunkData *chk, void *ptr);
void registerGlobal(ChunkData *chk, void *ptr, const char *cls, const char *name);
void writeChunk(ChunkData *chk, FILE *f);
/* the file's bytes in memory; the data is copied to a 16 byte aligned
 * buffer and fixed up, the file buffer is the caller's */
void *loadChunkMem(const uint8 *file, uint32 size, ChunkResolver resolve);
void *loadChunk(FILE *f, ChunkResolver resolve);
int isChunk(const uint8 *data, uint32 size);

#ifdef __cplusplus
}
#endif


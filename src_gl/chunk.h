
#ifdef __cplusplus
extern "C" {
#endif

typedef struct ChunkData ChunkData;
ChunkData *makeChunkData(void);
void freeChunkData(ChunkData *chk);
int registerBlock(ChunkData *chk, void *base, size_t size, int align);
int registerPointer(ChunkData *chk, void *ptr);
void writeChunk(ChunkData *chk, FILE *f);
void *loadChunk(FILE *f);

#ifdef __cplusplus
}
#endif


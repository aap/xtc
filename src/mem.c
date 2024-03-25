#include "mem.h"

#include "mdma.h"
#include "xtc.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

STRUCT(LLLink) {
	LLLink *next;
	LLLink *prev;
};
void linkInit(LLLink *link)
{
	link->next = nil;
	link->prev = nil;
}
void linkRemove(LLLink *link)
{
	link->prev->next = link->next;
	link->next->prev = link->prev;
}


STRUCT(LinkList) {
	LLLink link;
};
void listInit(LinkList *list)
{
	list->link.next = &list->link;
	list->link.prev = &list->link;
}
void listAdd(LinkList *list, LLLink *link)
{
	link->next = list->link.next;
	link->prev = &list->link;
	list->link.next->prev = link;
	list->link.next = link;
}

#define LLLinkGetData(linkvar,type,entry) \
	((type*)(((uint8*)(linkvar))-offsetof(type,entry)))


#define FORLIST(_link, _list) \
	for(LLLink *_next = nil, *_link = (_list).link.next; \
	_next = (_link)->next, (_link) != &(_list).link; \
	(_link) = _next)


STRUCT(MemoryBlock) {
	size_t sz;
	void *origPtr;
	LLLink inAllocList;
};
LinkList allocations;
size_t totalMemoryAllocated;
#define PTR2MEMBLOCK(p) (MemoryBlock*)((uint8*)p-16-sizeof(MemoryBlock))
#define MEMBLOCK2PTR(mem) (uint8*)((uint8*)mem+sizeof(MemoryBlock)+16)






void *malloc_managed(size_t sz);
void *realloc_managed(void *p, size_t sz);
void free_managed(void *p);

void
memInitManaged(void)
{
	listInit(&allocations);
	totalMemoryAllocated = 0;

	mdmaMalloc = malloc_managed;
	mdmaRealloc = realloc_managed;
	mdmaFree = free_managed;
}

#define ALIGN16(x) ((x) + 0xF & ~0xF) 
void*
malloc_managed(size_t sz)
{
	void *origPtr; 
	uint8 *data;
	MemoryBlock *mem;

	check_allocations();

	if(sz == 0) return nil;
	origPtr = malloc(sz + 32 + sizeof(MemoryBlock) + 15);
	if(origPtr == nil)
		return nil;
	totalMemoryAllocated += sz;
	data = (uint8*)origPtr;
	data += sizeof(MemoryBlock);
	data = (uint8*)ALIGN16((uintptr)data) + 16;
	mem = PTR2MEMBLOCK(data);
	memset(data-16, 0xCD, 16);
	memset(data+sz, 0xCD, 16);

	mem->sz = sz;
	mem->origPtr = origPtr;
	listAdd(&allocations, &mem->inAllocList);

	return data;
}

// not really tested yet
void*
realloc_managed(void *p, size_t sz)
{
	void *origPtr;
	MemoryBlock *mem;
	uint32 offset;

	if(p == nil)
		return malloc_managed(sz);

	check_allocations();

	mem = PTR2MEMBLOCK(p);
	offset = (uint8*)p - (uint8*)mem->origPtr;

	linkRemove(&mem->inAllocList);

	origPtr = realloc(mem->origPtr, sz + 32 + sizeof(MemoryBlock) + 15);
	if(origPtr == nil){
		listAdd(&allocations, &mem->inAllocList);
		return nil;
	}
	p = (uint8*)origPtr + offset;
	memset(p-16, 0xCD, 16);
	memset(p+sz, 0xCD, 16);
	mem = PTR2MEMBLOCK(p);
	totalMemoryAllocated -= mem->sz;
	mem->sz = sz;
	mem->origPtr = origPtr;
	listAdd(&allocations, &mem->inAllocList);
	totalMemoryAllocated += mem->sz;

	return p;
}

static void
checkblock(uint8 *p)
{
	MemoryBlock *mem;
	mem = PTR2MEMBLOCK(p);
	for(int i = 0; i < 16; i++) {
		if(p[i-16] != 0xCD) {
			printf("memory corruption before %p (%p)\n", p, &p[i-16]);
			for(i = 0; i < 16; i++) printf("%02X ", p[i-16]);
			printf("\n");
			exit(1);
		}
		if(p[mem->sz+i] != 0xCD) {
			printf("memory corruption after %p (%p)\n", p, &p[mem->sz+i]);
			for(i = 0; i < 16; i++) printf("%02X ", p[mem->sz+i]);
			printf("\n");
			exit(1);
		}
	}
}

void
free_managed(void *p)
{
	MemoryBlock *mem;
	if(p == nil)
		return;
	checkblock(p);
	mem = PTR2MEMBLOCK(p);
	linkRemove(&mem->inAllocList);
	totalMemoryAllocated -= mem->sz;
	free(mem->origPtr);
}

void
print_allocations(void)
{
	FORLIST(lnk, allocations) {
		MemoryBlock *mem = LLLinkGetData(lnk, MemoryBlock, inAllocList);
		printf("%d: %p - %p\n", mem->sz, mem->origPtr, (uint8*)mem->origPtr + mem->sz);
	}
}

void
check_allocations(void)
{
	FORLIST(lnk, allocations) {
		MemoryBlock *mem = LLLinkGetData(lnk, MemoryBlock, inAllocList);
		checkblock(MEMBLOCK2PTR(mem));
	}
}

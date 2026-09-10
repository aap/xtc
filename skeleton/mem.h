void memInitManaged(void);
void print_allocations(void);
void check_allocations(void);

/* the C heap, for a program that loads and frees: bytes handed out
 * right now, and how far the break has moved.  Used goes back to where
 * it was when everything is given back; size is a high water mark --
 * free() keeps its memory, it does not return it to the system. */
unsigned int memHeapUsed(void);
unsigned int memHeapSize(void);

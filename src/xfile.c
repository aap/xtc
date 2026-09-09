/* readfile over fio, for the model loaders in common/ */

#include "xtci.h"
#include "xmodel.h"	// declares readfile, with C linkage
#include "fio.h"
#include <stdio.h>
#include <stdlib.h>
#include <sifdev.h>

int
readfile(const char *path, uint8 **data, uint32 *size)
{
	int fd, sz, n;

	fd = fioOpen(path, SCE_RDONLY);
	if(fd < 0)
		return 0;
	sz = fioLseek(fd, 0, SCE_SEEK_END);
	fioLseek(fd, 0, SCE_SEEK_SET);
	*data = (uint8*)malloc(sz);
	n = fioRead(fd, *data, sz);
	fioClose(fd);
	if(n != sz) {
		printf("readfile: %s: %d of %d bytes\n", path, n, sz);
		free(*data);
		return 0;
	}
	*size = sz;
	return 1;
}

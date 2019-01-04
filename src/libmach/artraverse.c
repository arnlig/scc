#include <stdio.h>

#include <scc/ar.h>
#include <scc/mach.h>

int
artraverse(FILE *fp, int (*fn)(FILE *, char *, void *), void *data)
{
	int r;
	long off;
	fpos_t pos;
	char name[SARNAM+1];

	for (;;) {
		fgetpos(fp, &pos);

		if ((off = armember(fp, name)) < 0)
			return -1;
		r = !(*fn)(fp, name, data);
		if (!r)
			return r;

		fsetpos(fp, &pos);
		fseek(fp, off, SEEK_SET);

		if (off == 0)
			return 0;
	}
}
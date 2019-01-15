#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <scc/arg.h>
#include <scc/mach.h>

static int status;
static char *filename, *membname;
static int tflag;
static unsigned long long ttext, tdata, tbss, ttotal;
char *argv0;

static void
error(char *fmt, ...)
{
	va_list va;

	va_start(va, fmt);
	fprintf(stderr, "size: %s: ", filename);
	if (membname)
		fprintf(stderr, "%s: ", membname);
	vfprintf(stderr, fmt, va);
	putc('\n', stderr);
	va_end(va);

	status = EXIT_FAILURE;
}

void
newobject(FILE *fp, int type)
{
	Obj *obj;
	unsigned long long text, data, bss, total;

	if ((obj = objnew(type)) == NULL) {
		error("out of memory");
		goto error;
	}
	if (objread(obj, fp) < 0 || objsize(obj, &text, &data, &bss) < 0) {
		error("file corrupted");
		goto error;
	}

	total = text + data + bss;
	printf("%llu\t%llu\t%llu\t%llu\t%llx\t%s\n",
	       text, data, bss, total, total, filename);

	ttext += text;
	tdata += data;
	tbss += bss;
	ttotal += total;

error:
	if (obj)
		objdel(obj);
}

static int
newmember(FILE *fp, char *name, void *data)
{
	int t;

	membname = name;
	if ((t = objtype(fp, NULL)) != -1)
		newobject(fp, t);

	return 1;
}

static void
size(char *fname)
{
	int t;
	FILE *fp;

	filename = fname;
	if ((fp = fopen(fname, "rb")) == NULL) {
		error(strerror(errno));
		return;
	}

	if ((t = objtype(fp, NULL)) != -1)
		newobject(fp, t);
	else if (archive(fp))
		artraverse(fp, newmember, NULL);
	else
		error("bad format");

	if (ferror(fp))
		error(strerror(errno));

	fclose(fp);
}

static void
usage(void)
{
	fputs("usage: size [-t] [file...]\n", stderr);
	exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[])
{
	unsigned long long total;

	ARGBEGIN {
	case 't':
		tflag = 1;
		break;
	default:
		usage();
	} ARGEND

	puts("text\tdata\tbss\tdec\thex\tfilename");

	if (argc == 0) {
		size("a.out");
	} else {
		for (; *argv; ++argv)
			size(*argv);
	}

	if (tflag) {
		total = ttext + tdata + tbss;
		printf("%llu\t%llu\t%llu\t%llu\t%llx\t%s\n",
		       ttext, tdata, tbss, total, total, "(TOTALS)");
	}

	if (fflush(stdout)) {
		filename = "stdout";
		error(strerror(errno));
	}

	return status;
}
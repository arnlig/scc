static char sccsid[] = "@(#) ./ld/main.c";

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <scc/mach.h>
#include <scc/scc.h>
#include <scc/ar.h>
#include <scc/syslibs.h>

#define NR_SYMBOL 128

typedef struct objlst Objlst;
typedef struct symbol Symbol;
typedef struct section Section;

enum {
	NOINSTALL,
	INSTALL,
};

enum {
	OUTLIB,
	INLIB,
};

struct section {
	char *name;
	unsigned long long size, offset;
	unsigned flags;
	int type;
	FILE *fp;
	Section *next;
};

struct objlst {
	Obj *obj;
	struct objlst *next;
};

struct symbol {
	char *name;
	Obj *obj;
	Objsym *def;
	unsigned long long size, value;
	struct symbol *next, *prev;
	struct symbol *hash;
};

char *output = "a.out", *entry = "start", *datasiz;

static Section *sections;
static int bintype = -1;
static char *filename, *membname;
static Objlst *objhead, *objlast;
static Symbol *symtab[NR_SYMBOL];
static Symbol refhead = {
	.next = &refhead,
	.prev = &refhead,
};

static int sflag;		/* discard all the symbols */
static int xflag;		/* discard local symbols */
static int Xflag;		/* discard locals starting with 'L' */
static int rflag;		/* preserve relocation bits */
static int dflag;		/* define common even with rflag */
static int gflag;               /* preserve debug symbols */

static int status;

static char *
errstr(void)
{
	return strerror(errno);
}

static void
error(char *fmt, ...)
{
	va_list va;

	va_start(va, fmt);
	fprintf(stderr, "ld: %s: ", filename);
	if (membname)
		fprintf(stderr, "%s: ", membname);
	vfprintf(stderr, fmt, va);
	putc('\n', stderr);
	va_end(va);

	status = EXIT_FAILURE;
}

static void
cleanup(void)
{
	if (status != EXIT_FAILURE)
		remove(output);
}

static int
moreundef(void)
{

	return refhead.next != &refhead;
}

static Symbol *
lookup(char *name, int install)
{
	size_t len;
	char *s;
	unsigned h;
	Symbol *sym;

	h = genhash(name) % NR_SYMBOL;

	for (sym = symtab[h]; sym; sym = sym->hash) {
		if (!strcmp(name, sym->name))
			return sym;
	}

	if (!install)
		return NULL;

	len = strlen(name) + 1;
	sym = malloc(sizeof(*sym));
	s = malloc(len);
	if (!len || !s) {
		error("out of memory");
		exit(EXIT_FAILURE);
	}

	sym->obj = NULL;
	sym->name = memcpy(s, name, len);
	sym->hash = symtab[h];
	symtab[h] = sym;
	sym->value = 0;
	sym->size = 0;

	refhead.next->prev = sym;
	sym->next = refhead.next;
	refhead.next = sym;
	sym->prev = &refhead;

	return sym;
}

static Symbol *
define(Objsym *osym, Obj *obj)
{
	Symbol *sym = lookup(osym->name, INSTALL);

	if (sym->def && sym->def->type != 'C') {
		error("%s: symbol redefined", osym->name);
		return NULL;
	}

	sym->obj = obj;
	sym->def = osym;
	sym->size = osym->size;
	sym->value = osym->value;

	sym->next->prev = sym->prev;
	sym->prev->next = sym->next;
	sym->next = sym->prev = NULL;

	return sym;
}

static int
newsym(Objsym *osym, Obj *obj)
{
	Symbol *sym;

	switch (osym->type) {
	case 'U':
		lookup(osym->name, INSTALL);
	case '?':
	case 'N':
		break;
	case 'C':
		sym = lookup(osym->name, NOINSTALL);
		if (!sym || !sym->def) {
			define(osym, obj);
			break;
		}
		if (sym->def->type != 'C')
			break;
		if (sym->size < osym->size)
			sym->size = osym->size;
		break;
	default:
		if (isupper(osym->type))
			define(osym, obj);
		break;
	}

	return 1;
}

static void
copy(FILE *to, FILE *from, long pad, long nbytes)
{
	int c;

	while (pad--)
		putc(0, to);

	while (nbytes-- && (c = getc(from)) != EOF)
		putc(c, to);

	if (c == EOF) {
		error("section truncated");
		exit(EXIT_FAILURE);
	}

	if (ferror(to) || ferror(from)) {
		error(errstr());
		exit(EXIT_FAILURE);
	}
}

static Section *
findsect(Objsect *secp)
{
	size_t len;
	char *s;
	FILE *fp;
	Section *sp, *lastp;

	for (lastp = sp = sections; sp; lastp = sp, sp = sp->next) {
		if (!strcmp(sp->name, secp->name))
			return sp;
	}

	len = strlen(secp->name) + 1;
	s = malloc(len);
	fp = tmpfile();

	sp = malloc(sizeof(*sp));
	if (!s || !sp || !fp) {
		error(errstr());
		exit(EXIT_FAILURE);
	}

	if (lastp) {
		lastp->next = sp;
	} else {
		sections = sp;
		sp->next = NULL;
	}

	sp->name = memcpy(s, secp->name, len);
	sp->offset = sp->size = 0;
	sp->fp = fp;
	sp->flags = secp->flags;
	sp->type = secp->type;

	return sp;
}

extern int objpos(Obj *obj, FILE *fp, long pos);

static void
newsect(Objsect *secp, Obj *obj, FILE *fp)
{
	unsigned long long align, size, pad, off;
	Section *sp;

	sp = findsect(secp);

	align = secp->align - 1;
	pad = (sp->size+align) & ~align;

	if (sp->size > ULLONG_MAX - pad)
		goto overflow;
	off = sp->size += pad;

	if (sp->size > ULLONG_MAX - secp->size)
		goto overflow;
	sp->size += secp->size;

	objpos(obj, fp, secp->offset);
	copy(sp->fp, fp, pad, secp->size);

	/*
	 * and now update the offset to relect the offset
	 * in the output file
	 */
	secp->offset = off;

	return;

overflow:
	error("section overflow");
	exit(EXIT_FAILURE);
}

static void
loadobj(Obj *obj, FILE *fp)
{
	int n;
	Objlst *lst;
	Objsym *sym;
	Objsect *secp;

	if ((lst = malloc(sizeof(*lst))) == NULL) {
		error("out of memory");
		return;
	}

	lst->obj = obj;
	lst->next = NULL;

	if (!objlast)
		objlast = objhead = lst;
	else
		objlast = objlast->next = lst;

	for (sym = obj->syms; sym; sym = sym->next)
		newsym(sym, obj);

	for (secp = obj->secs; secp; secp = secp->next)
		newsect(secp, obj, fp);
}

static void
newobject(FILE *fp, int type, int inlib)
{
	Obj *obj;
	Symbol *sym, *p;

	if ((obj = objnew(type)) == NULL) {
		error("out of memory");
		return;
	}

	if (bintype == -1) {
		bintype = type;
	} else if (bintype != type) {
		error("not compatible object file");
		goto delete;
	}
	bintype = type;

	if (objread(obj, fp) < 0) {
		error("object file corrupted");
		goto delete;
	}

	if (objsyms(obj) < 0 || objsect(obj) < 0) {
		error("object file corrupted");
		goto delete;
	}

	if (!inlib) {
		loadobj(obj, fp);
		return;
	}

	/*
	 * we are in a library without index, so we have to check
	 * if it defines some symbol that is undefined and only
	 * in that case we have to load the object
	 */
	p = &refhead;
	for (sym = p->next; sym != p; sym = sym->next) {
		if (objlookup(obj, sym->name, 0)) {
			loadobj(obj, fp);
			return;
		}
	}

delete:
	objdel(obj);
	return;
}

static void
loadlib(FILE *fp)
{
	int t, loaded;
	long n;
	Objsymdef *def, *dp;
	Symbol *sym;

	if (getindex(bintype, &n, &def, fp) < 0) {
		error("corrupted index");
		return;
	}

	loaded = 1;
	while (moreundef() && loaded) {
		loaded = 0;
		for (dp = def; dp; dp = dp->next) {
			sym = lookup(dp->name, NOINSTALL);
			if (!sym || sym->def)
				continue;

			if (fseek(fp, dp->offset, SEEK_SET) == EOF) {
				error(errstr());
				goto clean;
			}

			if ((t = objtype(fp, NULL)) == -1) {
				error("library file corrupted");
				goto clean;
			}

			if (t != bintype) {
				error("incompatible library");
				goto clean;
			}

			newobject(fp, t, OUTLIB);
			loaded = 1;
		}
	}
clean:
	free(def);
}

static int
newmember(FILE *fp, char *name, void *data)
{
	int t;
	int *nmemb = data;

	if (bintype == -1) {
		error("an object file is needed before any library");
		return 0;
	}

	if (*nmemb++ == 0) {
		if (!strncmp(name, "/", SARNAM) ||
		    !strncmp(name, "__.SYMDEF", SARNAM)) {
			loadlib(fp);
			return 0;
		}
	}

	membname = name;
	if ((t = objtype(fp, NULL)) == -1)
		return 1;

	if (bintype != t) {
		error("wrong object file format");
		return 1;
	}

	newobject(fp, t, INLIB);
	membname = NULL;

	return 1;
}

static int
newlibrary(FILE *fp)
{
	int nmemb = 0;

	return formember(fp, newmember, &nmemb);
}

static FILE *
openfile(char *name, char *buffer)
{
	size_t pathlen, len;
	FILE *fp;
	char **bp, **base, **end;
	char libname[FILENAME_MAX];

	filename = name;
	membname = NULL;
	if (name[0] != '-' || name[1] != 'l') {
		if ((fp = fopen(name, "rb")) == NULL)
			error(errstr());
		return fp;
	}

	len = strlen(name+2) + 3;
	if (len > FILENAME_MAX-1) {
		error("library name too long");
		return NULL;
	}
	strcat(strcpy(buffer, "lib"), name+2);

	filename = buffer;
	if ((fp = fopen(libname, "rb")) != NULL)
		return fp;

	base = syslibs;
	end = &syslibs[MAX_LIB_PATHS];
	for (bp = base; bp < end && *bp; ++bp) {
		pathlen = strlen(*bp);
		if (pathlen + len > FILENAME_MAX-1)
			continue;
		memcpy(libname, *bp, pathlen);
		memcpy(libname+pathlen+1, buffer, len);
		buffer[pathlen] = '/';

		if ((fp = fopen(buffer, "rb")) != NULL)
			return fp;
	}

	error("not found");
	return NULL;
}

static void
listundef(void)
{
	Symbol *sym, *p;

	p = &refhead;
	for (sym = p->next; sym != p; sym = sym->next) {
		fprintf(stderr,
		        "ld: symbol '%s' not defined\n",
		        sym->name);
	}
}

static void
pass1(int argc, char *argv[])
{
	int t;
	FILE *fp;
	char buff[FILENAME_MAX];

	for ( ; *argv; ++argv) {
		if ((fp = openfile(*argv, buff)) == NULL)
			continue;

		if ((t = objtype(fp, NULL)) != -1)
			newobject(fp, t, OUTLIB);
		else if (archive(fp))
			newlibrary(fp);
		else
			error("bad format");

		fclose(fp);
	}

	if (moreundef()) {
		listundef();
		exit(EXIT_FAILURE);
	}
}

/*
 * default memory layout:
 * -text
 * -data
 * -bss
 */
static void
pass2(int argc, char *argv[])
{
	FILE *fp;
	Section *sp;
	long off;
	unsigned long long addr;

	if ((fp = fopen("binary", "wb")) == NULL) {
		perror("opening output");
		exit(EXIT_FAILURE);
	}

	addr = 0x100;
	for (sp = sections; sp; sp = sp->next) {
		fprintf(stderr, "1st - %c\n", sp->type);
		if (sp->type != 'T')
			continue;
		rewind(sp->fp);
		copy(fp, sp->fp, 0, sp->size);
		addr += sp->size;
		fclose(sp->fp);
	}

	addr = addr+3 & ~3;
	for (sp = sections; sp; sp = sp->next) {
		fprintf(stderr, "2nd - %c\n", sp->type);
		if (sp->type != 'D')
			continue;
		rewind(sp->fp);
		copy(fp, sp->fp, 0, sp->size);
		addr += sp->size;
		fclose(sp->fp);
	}

	addr = addr+3 & ~3;
	for (sp = sections; sp; sp = sp->next) {
		fprintf(stderr, "3rd - %c\n", sp->type);
		if (sp->type != 'B')
			continue;
		addr += sp->size;
		fclose(sp->fp);
	}
}

static void
usage(void)
{
	fputs("usage: ld [options] file ...\n", stderr);
	exit(EXIT_FAILURE);
}

static void
Lpath(char *path)
{
	char **bp, **base, **end;

	base = syslibs;
	end = &syslibs[MAX_LIB_PATHS];
	for (bp = base; bp < end && *bp; ++bp)
		;
	if (bp == end) {
		fputs("ld: too many -L options\n", stderr);
		exit(1);
	}
	*bp = path;
}

int
main(int argc, char *argv[])
{
	char *cp, **p;

	for (--argc; *++argv; --argc) {
		if (argv[0][0] != '-' || argv[0][1] == 'l')
			break;
		if (argv[0][1] == '-') {
			--argc, ++argv;
			break;
		}
		for (cp = &argv[0][1]; *cp; ++cp) {
			switch (*cp) {
			case 's':
				sflag = 1;
				break;
			case 'x':
				xflag = 1;
				break;
			case 'X':
				Xflag = 1;
				break;
			case 'r':
				rflag = 1;
				break;
			case 'd':
				dflag = 1;
				break;
			case 'i':
			case 'n':
				/* TODO */
				break;
			case 'L':
				if (argc == 0)
					goto usage;
				++argv, --argc;
				Lpath(*argv);
				break;
			case 'u':
				if (argc == 0)
					goto usage;
				++argv, --argc;
				lookup(*argv, INSTALL);
				break;
			case 'o':
				if (argc == 0)
					goto usage;
				++argv, --argc;
				output = *argv;
				break;
			case 'e':
				if (argc == 0)
					goto usage;
				++argv, --argc;
				entry = *argv;
				break;
			case 'D':
				if (argc == 0)
					goto usage;
				++argv, --argc;
				datasiz = *argv;
				break;
			default:
			usage:
				usage();
			}
		}
	}

	if (argc == 0)
		usage();

	atexit(cleanup);

	pass1(argc, argv);
	pass2(argc, argv);

	return status;
}

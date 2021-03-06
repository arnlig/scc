#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <scc/arg.h>
#include <scc/scc.h>
#include "cc1.h"

char *argv0, *infile;

int warnings;
jmp_buf recover;

static struct items uflags;
int onlycpp, onlyheader;


extern int failure;

static void
defmacro(char *macro)
{
	char *p = strchr(macro, '=');

	if (p)
		*p++ = '\0';
	else
		p = "1";

	defdefine(macro, p, "command-line");
}

static void
usage(void)
{
	fputs("usage: cc1 [-Ewd] [-D def[=val]]... [-U def]... "
	      "[-I dir]... [-o output] [input]\n", stderr);
	exit(1);
}

int
main(int argc, char *argv[])
{
	int i;

	ilex();
	icpp();
	icode();
	ibuilts();

	ARGBEGIN {
	case 'D':
		defmacro(EARGF(usage()));
		break;
	case 'M':
		onlyheader = 1;
		break;
	case 'E':
		onlycpp = 1;
		break;
	case 'I':
		incdir(EARGF(usage()));
		break;
	case 'U':
		newitem(&uflags, EARGF(usage()));
		break;
	case 'd':
		DBGON();
		break;
	case 'w':
		warnings = 1;
		break;
	default:
		usage();
	} ARGEND

	if (argc > 1)
		usage();

	for (i = 0; i < uflags.n; ++i)
		undefmacro(uflags.s[i]);

	infile = (*argv) ? *argv : "<stdin>";
	addinput(*argv, NULL, NULL);

	/*
	 * we cannot initialize arch until we have an
	 * output stream, because we maybe want to emit new types
	 */
	iarch();
	if (onlycpp || onlyheader) {
		outcpp();
	} else {
		for (next(); yytoken != EOFTOK; decl())
			;
	}

	return failure;
}

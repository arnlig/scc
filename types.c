
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "sizes.h"
#include "cc.h"
#include "tokens.h"
#include "symbol.h"


static unsigned char stack[NR_DECLARATORS];
static unsigned char *stackp = stack;

struct qualifier *
initqlf(struct qualifier *qlf)
{
	memset(qlf, 0, sizeof(*qlf));
	return qlf;
}

struct ctype *
initctype(register struct ctype *tp)
{
	memset(tp, 0, sizeof(*tp));
	tp->type = INT;
	tp->forward = 1;
	return tp;
}

struct storage *
initstore(register struct storage *store)
{
	extern unsigned char curctx;
	memset(store, 0, sizeof(*store));

	if (curctx != CTX_OUTER)
		store->c_auto = 1;
	return store;
}

void
delctype(register struct ctype *tp)
{
	if (!tp)
		return;
	if (tp->base)
		delctype(tp->base);
	free(tp);
}

static struct ctype *
mktype(register struct ctype *tp, unsigned  char op)
{
	unsigned len;

	switch (op) {
	case ARY:
		assert(stackp != stack);
		len = *--stackp;
	case PTR: case FTN: {
		register struct ctype *aux = tp;

		tp = xmalloc(sizeof(*tp));
		initctype(tp);
		tp->type = op;
		tp->base = aux;
		tp->len = len;
		break;
	}
	case VOLATILE:
		tp->c_volatile = 1;
		break;
	case RESTRICT:
		tp->c_restrict = 1;
		break;
	case CONST:
		tp->c_const = 1;
		break;
	default:
		assert(0);
	}
	return tp;
}

void
pushtype(unsigned mod)
{
	if (stackp == stack + NR_DECLARATORS)
		error("Too much type declarators");
	*stackp++ = mod;
}

struct ctype *
decl_type(struct ctype *tp)
{
	struct ctype *new = xmalloc(sizeof(*new));
	*new = *tp;

	while (stackp != stack)
		new = mktype(new, *--stackp);
	return new;
}

struct ctype *
ctype(struct ctype *tp, unsigned char tok)
{
	register unsigned char type;

	if (!tp->defined) {
		tp->type = 0;
		tp->defined = 1;
	}
	type = tp->type;


	switch (tok) {
	case VOID: case BOOL: case STRUCT: case UNION: case ENUM: case BITFLD:
		if (type)
			goto two_or_more;;
		type = tok;
		if (tp->c_signed || tp->c_unsigned)
			goto invalid_sign;
		break;
	case CHAR:
		if (type)
			goto two_or_more;
		type = CHAR;
		break;
	case SHORT:
		if (type && type != INT)
			goto two_or_more;
		type = SHORT;
		break;
	case INT:
		switch (type) {
		case 0:       type = INT;       break;
		case SHORT:   type = SHORT;     break;
		case LONG:    type = LONG;      break;
		default:      goto two_or_more;
		}
		break;
	case LONG:
		switch (type) {
		case 0:
		case INT:     type = LONG;      break;
		case LONG:    type = LLONG;     break;
		case DOUBLE:  type = LDOUBLE;   break;
		case LLONG:
		case LDOUBLE:  error("too much long");
		}
		break;
	case FLOAT:
		if (type)
			goto two_or_more;
		type = FLOAT;
		if (tp->c_signed || tp->c_unsigned)
			goto check_sign;
		break;
	case DOUBLE:
		if (type)
			goto two_or_more;
		if (!type)
			type = DOUBLE;
		else if (type == LONG)
			type = LDOUBLE;
		if (tp->c_signed || tp->c_unsigned)
			goto check_sign;
		break;
	case UNSIGNED:
		if (tp->c_unsigned)
			goto duplicated;
		if (tp->c_signed)
			goto both_sign;
		tp->c_unsigned = 1;
		goto check_sign;
	case SIGNED:
		if (tp->c_signed)
			goto duplicated;
		if (tp->c_unsigned)
			goto both_sign;
		tp->c_signed = 1;

check_sign:	switch (type) {
		case VOID: case BOOL: case STRUCT: case UNION: case ENUM:
			goto invalid_sign;
		}
		break;
	case TYPEDEF:
		assert(!type);
		if (tp->c_signed || tp->c_unsigned)
			goto invalid_sign;
		type = TYPEDEF;
		break;
	default:
		assert(0);
	}
	tp->type = type;
	return tp;

both_sign:
	error("both 'signed' and 'unsigned' in declaration specifiers");
duplicated:
	error("duplicated '%s'", yytext);
invalid_sign:
	error("invalid sign modifier");
two_or_more:
	error("two or more basic types");
}

struct qualifier *
qualifier(register struct qualifier *qlf, unsigned char mod)
{
	switch (mod) {
	case CONST:
		if (options.repeat && qlf->c_const)
			goto duplicated;
		qlf->c_const = 1;
		break;
	case VOLATILE:
		if (options.repeat && qlf->c_volatile)
			goto duplicated;
		qlf->c_volatile = 1;
		break;
	default:
		assert(0);
	}

	qlf->defined = 1;
	return qlf;
duplicated:
	error("duplicated '%s'", yytext);
}

struct storage *
storage(register struct storage *sp, unsigned char mod)
{
	extern unsigned char curctx;

	if (!sp->defined)
		sp->c_auto = 0;
	else
		error("Two or more storage specifier");

	switch (mod) {
	case TYPEDEF:
		sp->c_typedef = 1;
		break;
	case EXTERN:
		sp->c_extern = 1;
		break;
	case STATIC:
		sp->c_static = 1;
		break;
	case AUTO:
		if (curctx == CTX_OUTER)
			goto bad_file_scope_storage;
		sp->c_auto = 1;
		break;
	case REGISTER:
		if (curctx == CTX_OUTER)
			goto bad_file_scope_storage;
		sp->c_register = 1;
		break;
	default:
		assert(0);
	}
	sp->defined = 1;
	return sp;

bad_file_scope_storage:
	error("file-scope declaration specifies '%s'", yytext);
}

#ifndef NDEBUG
#include <stdio.h>

void
ptype(register struct ctype *tp)
{
	static const char *strings[] = {
		[0] =        "[no type]",
		[ARY] =      "array of ",
		[PTR] =      "pointer to ",
		[FTN] =      "function that returns ",
		[VOLATILE] = "volatile ",
		[RESTRICT] = "restrict ",
		[CONST] =    "const ",
		[INT] =      "int ",
		[CHAR] =     "char ",
		[FLOAT] =    "float ",
		[LONG] =     "long ",
		[LLONG] =    "long long ",
		[SHORT] =    "short ",
		[VOID] =     "void ",
		[DOUBLE] =   "double ",
		[LDOUBLE] =  "long double "
	};
	assert(tp);

	for (; tp; tp = tp->base)
		fputs(strings[tp->type], stdout);
	putchar('\n');
}

#endif

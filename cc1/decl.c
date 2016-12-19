/* See LICENSE file for copyright and license details. */
static char sccsid[] = "@(#) ./cc1/decl.c";
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cstd.h>
#include "../inc/cc.h"
#include "cc1.h"

#define NOSCLASS  0

#define NOREP 0
#define REP 1


struct declarators {
	unsigned char nr;
	struct declarator {
		unsigned char op;
		TINT  nelem;
		Symbol *sym;
		Type **tpars;
		Symbol **pars;
	} d [NR_DECLARATORS];
};

struct decl {
	unsigned ns;
	int sclass;
	int qualifier;
	Symbol *sym;
	Symbol **pars;
	Type *type;
	Type *parent;
};

static void
push(struct declarators *dp, int op, ...)
{
	va_list va;
	unsigned n;
	struct declarator *p;

	va_start(va, op);
	if ((n = dp->nr++) == NR_DECLARATORS)
		error("too many declarators");

	p = &dp->d[n];
	p->op = op;
	p->tpars = NULL;

	switch (op) {
	case ARY:
		p->nelem = va_arg(va, TINT);
		break;
	case KRFTN:
	case FTN:
		p->nelem = va_arg(va, TINT);
		p->tpars = va_arg(va, Type **);
		p->pars = va_arg(va, Symbol **);
		break;
	case IDEN:
		p->sym = va_arg(va, Symbol *);
		break;
	}
	va_end(va);
}

static int
pop(struct declarators *dp, struct decl *dcl)
{
	struct declarator *p;

	if (dp->nr == 0)
		return 0;

	p = &dp->d[--dp->nr];
	if (p->op == IDEN) {
		dcl->sym = p->sym;
		return 1;
	}
	if (dcl->type->op == FTN) {
		/*
		 * constructor applied to a
		 * function. We  don't need
		 * the parameter symbols anymore.
		 */
		free(dcl->pars);
		popctx();
		dcl->pars = NULL;
	}
	if (p->op == FTN || p->op == KRFTN)
		dcl->pars = p->pars;
	dcl->type = mktype(dcl->type, p->op, p->nelem, p->tpars);
	return 1;
}

static void
arydcl(struct declarators *dp)
{
	Node *np = NULL;
	TINT n = 0;

	expect('[');
	if (yytoken != ']') {
		if ((np = iconstexpr()) == NULL) {
			errorp("invalid storage size");
		} else {
			if ((n = np->sym->u.i) <= 0) {
				errorp("array size is not a positive number");
				n = 1;
			}
			freetree(np);
		}
	}
	expect(']');

	push(dp, ARY, n);
}

static int
empty(Symbol *sym, Type *tp, int param)
{
	if (!sym->name) {
		sym->type = tp;
		switch (tp->op) {
		default:
			 /* warn if it is not a parameter */
			if (!param)
				warn("empty declaration");
		case STRUCT:
		case UNION:
		case ENUM:
			return 1;
		}
	}
	return 0;
}

static Symbol *
parameter(struct decl *dcl)
{
	Symbol *sym = dcl->sym;
	Type *funtp = dcl->parent, *tp = dcl->type;
	TINT n = funtp->n.elem;
	char *name = sym->name;
	int flags;

	flags = 0;
	switch (dcl->sclass) {
	case STATIC:
	case EXTERN:
	case AUTO:
		errorp("bad storage class in function parameter");
		break;
	case REGISTER:
		flags |= SREGISTER;
		break;
	case NOSCLASS:
		flags |= SAUTO;
		break;
	}

	switch (tp->op) {
	case VOID:
		if (n != 0 || (funtp->prop & TK_R)) {
			errorp("incorrect void parameter");
			return NULL;
		}
		funtp->n.elem = -1;
		if (dcl->sclass)
			errorp("void as unique parameter may not be qualified");
		return NULL;
	case ARY:
		tp = mktype(tp->type, PTR, 0, NULL);
		break;
	case FTN:
		errorp("incorrect function type for a function parameter");
		return NULL;
	}
	if (!empty(sym, tp, 1)) {
		Symbol *p = install(NS_IDEN, sym);
		if (!p && !(funtp->prop & TK_R)) {
			errorp("redefinition of parameter '%s'", name);
			return NULL;
		}
		if (p && (funtp->prop & TK_R)) {
			errorp("declaration for parameter ‘%s’ but no such parameter",
			       sym->name);
			return NULL;
		}
		if (p)
			sym = p;
	}

	sym->type = tp;
	sym->flags &= ~(SAUTO|SREGISTER);
	sym->flags |= flags;
	return sym;
}

static Symbol *dodcl(int rep,
                     Symbol *(*fun)(struct decl *),
                     unsigned ns,
                     Type *type);

static void
krfun(Type *tp, Type *types[], Symbol *syms[], int *ntypes, int *nsyms)
{
	int n = 0;
	Symbol *sym;
	int toomany = 0;

	if (yytoken != ')') {
		do {
			sym = yylval.sym;
			expect(IDEN);
			sym->type = inttype;
			sym->flags |= SAUTO;
			if ((sym = install(NS_IDEN, sym)) == NULL) {
				errorp("redefinition of parameter '%s'",
				       yylval.sym->name);
				continue;
			}
			if (n < NR_FUNPARAM) {
				++n;
				*syms++ = sym;
				continue;
			}
			if (!toomany)
				errorp("too much parameters in function definition");
			toomany = 1;
		} while (accept(','));
	}

	*nsyms = n;
	*ntypes = 1;
	types[0] = ellipsistype;
}

static void
ansifun(Type *tp, Type *types[], Symbol *syms[], int *ntypes, int *nsyms)
{
	int n = 0;
	Symbol *sym;
	int toomany = 0, toovoid = 0;

	do {
		if (n == -1 && !toovoid) {
			errorp("'void' must be the only parameter");
			toovoid = 1;
		}
		if (accept(ELLIPSIS)) {
			if (n == 0)
				errorp("a named argument is requiered before '...'");
			++n;
			*syms = NULL;
			*types++ = ellipsistype;
			break;
		}
		if ((sym = dodcl(NOREP, parameter, NS_IDEN, tp)) == NULL)
			continue;
		if (tp->n.elem == -1) {
			n = -1;
			continue;
		}
		if (n < NR_FUNPARAM) {
			*syms++ = sym;
			*types++ = sym->type;
			++n;
			continue;
		}
		if (!toomany)
			errorp("too many parameters in function definition");
		toomany = 1;
	} while (accept(','));

	*nsyms = n;
	*ntypes = n;
}

static void
fundcl(struct declarators *dp)
{
	Type *types[NR_FUNPARAM], type;
	Symbol *syms[NR_FUNPARAM+1], **pars;
	int k_r, ntypes, nsyms;
	size_t size;

	pushctx();
	expect('(');
	type.n.elem = 0;
	type.prop = 0;

	k_r = (yytoken == ')' || yytoken == IDEN);
	(k_r ? krfun : ansifun)(&type, types, syms, &ntypes, &nsyms);
	expect(')');

	type.n.elem = ntypes;
	if (ntypes <= 0) {
		type.p.pars = NULL;
	} else {
		size = ntypes * sizeof(Type *);
		type.p.pars = memcpy(xmalloc(size), types, size);
	}
	if (nsyms <= 0) {
		pars = NULL;
	} else {
		size = (nsyms + 1) * sizeof(Symbol *);
		pars = memcpy(xmalloc(size), syms, size);
		pars[nsyms] = NULL;
	}
	push(dp, (k_r) ? KRFTN : FTN, type.n.elem, type.p.pars, pars);
}

static void declarator(struct declarators *dp, unsigned ns);

static void
directdcl(struct declarators *dp, unsigned ns)
{
	Symbol *sym;
	static int nested;

	if (accept('(')) {
		if (nested == NR_SUBTYPE)
			error("too many declarators nested by parentheses");
		++nested;
		declarator(dp, ns);
		--nested;
		expect(')');
	} else {
		if (yytoken == IDEN || yytoken == TYPEIDEN) {
			sym = yylval.sym;
			next();
		} else {
			sym = newsym(ns);
		}
		push(dp, IDEN, sym);
	}

	for (;;) {
		switch (yytoken) {
		case '(':  fundcl(dp); break;
		case '[':  arydcl(dp); break;
		default:   return;
		}
	}
}

static void
declarator(struct declarators *dp, unsigned ns)
{
	unsigned  n;

	for (n = 0; accept('*'); ++n) {
		while (accept(TQUALIFIER))
			/* nothing */;
	}

	directdcl(dp, ns);

	while (n--)
		push(dp, PTR);
}

static Type *structdcl(void), *enumdcl(void);

static Type *
specifier(int *sclass, int *qualifier)
{
	Type *tp = NULL;
	unsigned spec, qlf, sign, type, cls, size;

	spec = qlf = sign = type = cls = size = 0;

	for (;;) {
		unsigned *p = NULL;
		Type *(*dcl)(void) = NULL;

		switch (yytoken) {
		case SCLASS:
			p = &cls;
			break;
		case TQUALIFIER:
			qlf |= yylval.token;
			next();
			continue;
		case TYPEIDEN:
			if (type)
				goto return_type;
			tp = yylval.sym->type;
			p = &type;
			break;
		case TYPE:
			switch (yylval.token) {
			case ENUM:
				dcl = enumdcl;
				p = &type;
				break;
			case STRUCT:
			case UNION:
				dcl = structdcl;
				p = &type;
				break;
			case VA_LIST:
			case VOID:
			case BOOL:
			case CHAR:
			case INT:
			case FLOAT:
			case DOUBLE:
				p = &type;
				break;
			case SIGNED:
			case UNSIGNED:
				p = &sign;
				break;
			case LONG:
				if (size == LONG) {
					yylval.token = LLONG;
					size = 0;
				}
			case SHORT:
				p = &size;
				break;
			}
			break;
		default:
			goto return_type;
		}
		if (*p)
			errorp("invalid type specification");
		*p = yylval.token;
		if (dcl) {
			if (size || sign)
				errorp("invalid type specification");
			tp = (*dcl)();
			goto return_type;
		} else {
			next();
		}
		spec = 1;
	}

return_type:
	*sclass = cls;
	*qualifier = qlf;
	if (!tp) {
		if (spec) {
			tp = ctype(type, sign, size);
		} else {
			if (curctx != GLOBALCTX)
				unexpected();
			warn("type defaults to 'int' in declaration");
			tp = inttype;
		}
	}
	return tp;
}

static Symbol *
newtag(void)
{
	Symbol *sym;
	int op, tag = yylval.token;
	static unsigned ns = NS_STRUCTS;

	namespace = NS_TAG;
	next();

	switch (yytoken) {
	case IDEN:
	case TYPEIDEN:
		sym = yylval.sym;
		if ((sym->flags & SDECLARED) == 0)
			install(NS_TAG, yylval.sym);
		namespace = NS_IDEN;
		next();
		break;
	default:
		sym = newsym(NS_TAG);
		break;
	}
	if (!sym->type) {
		Type *tp;

		if (ns == NS_STRUCTS + NR_MAXSTRUCTS)
			error("too many tags declared");
		tp = mktype(NULL, tag, 0, NULL);
		tp->ns = ns++;
		sym->type = tp;
		tp->tag = sym;
		DBG("declared tag '%s' with ns = %d\n",
		    (sym->name) ? sym->name : "anonymous", tp->ns);
	}

	if ((op = sym->type->op) != tag &&  op != INT)
		error("'%s' defined as wrong kind of tag", sym->name);
	return sym;
}

/* TODO: bitfields */

static void fieldlist(Type *tp);

static Type *
structdcl(void)
{
	Symbol *sym;
	Type *tp;
	static int nested;
	int ns;

	ns = namespace;
	sym = newtag();
	tp = sym->type;
	namespace = tp->ns;

	if (!accept('{')) {
		namespace = ns;
		return tp;
	}

	if (tp->prop & TDEFINED && sym->ctx == curctx)
		error("redefinition of struct/union '%s'", sym->name);

	if (nested == NR_STRUCT_LEVEL)
		error("too many levels of nested structure or union definitions");

	++nested;
	while (yytoken != '}') {
		fieldlist(tp);
	}
	--nested;

	deftype(tp);
	namespace = ns;
	expect('}');
	return tp;
}

static Type *
enumdcl(void)
{
	Type *tp;
	Symbol *sym, *tagsym;
	int ns, val, toomany;
	unsigned nctes;

	ns = namespace;
	tagsym = newtag();
	tp = tagsym->type;

	if (!accept('{'))
		goto restore_name;
	if (tp->prop & TDEFINED)
		errorp("redefinition of enumeration '%s'", tagsym->name);
	deftype(tp);
	namespace = NS_IDEN;

	/* TODO: check incorrect values in val */
	for (nctes = val = 0; yytoken != '}'; ++nctes, ++val) {
		if (yytoken != IDEN)
			unexpected();
		sym = yylval.sym;
		next();
		if (nctes == NR_ENUM_CTES && !toomany) {
			errorp("too many enum constants in a single enum");
			toomany = 1;
		}
		if (accept('=')) {
			Node *np = iconstexpr();

			if (np == NULL)
				errorp("invalid enumeration value");
			else
				val = np->sym->u.i;
			freetree(np);
		}
		if ((sym = install(NS_IDEN, sym)) == NULL) {
			errorp("'%s' redeclared as different kind of symbol",
			       yytext);
		} else {
			sym->u.i = val;
			sym->flags |= SCONSTANT;
			sym->type = inttype;
		}
		if (!accept(','))
			break;
	}
	expect('}');

restore_name:
	namespace = ns;
	return tp;
}

static Symbol *
type(struct decl *dcl)
{
	Symbol *sym = dcl->sym;

	if (dcl->sclass)
		error("class storage in type name");
	if (sym->name)
		error("unexpected identifier in type name");
	sym->type = dcl->type;

	return sym;
}

static Symbol *
field(struct decl *dcl)
{
	static char *anon = "<anonymous>";
	Symbol *sym = dcl->sym;
	char *name = (sym->name) ? sym->name : anon;
	Type *structp = dcl->parent, *tp = dcl->type;
	TINT n = structp->n.elem;
	int err = 0;

	if (accept(':')) {
		Node *np;
		TINT n;

		if ((np = iconstexpr()) == NULL) {
			unexpected();
			n = 0;
		} else {
			n = np->sym->u.i;
			freetree(np);
		}
		if (n == 0 && name != anon)
			errorp("zero width for bit-field '%s'", name);
		if (tp != booltype && tp != inttype && tp != uinttype)
			errorp("bit-field '%s' has invalid type", name);
		if (n < 0)
			errorp("negative width in bit-field '%s'", name);
		else if (n > tp->size*8)
			errorp("width of '%s' exceeds its type", name);
	} else if (empty(sym, tp, 0)) {
		return sym;
	}

	if (tp->op == FTN) {
		errorp("invalid type '%s' in struct/union", name);
		err = 1;
	}
	if (dcl->sclass) {
		errorp("storage class in struct/union field '%s'", name);
		err = 1;
	}
	if (!(tp->prop & TDEFINED)) {
		error("field '%s' has incomplete type", name);
		err = 1;
	}
	if (err)
		return sym;

	if ((sym = install(dcl->ns, sym)) == NULL)
		error("duplicated member '%s'", name);
	sym->type = tp;

	sym->flags |= SFIELD;
	if (n == NR_FIELDS)
		error("too many fields in struct/union");
	DBG("New field '%s' in namespace %d\n", name, structp->ns);
	structp->p.fields = xrealloc(structp->p.fields, ++n * sizeof(*sym));
	structp->p.fields[n-1] = sym;
	structp->n.elem = n;

	return sym;
}

static void
bad_storage(Type *tp, char *name)
{
	if (tp->op != FTN)
		errorp("incorrect storage class for file-scope declaration");
	else
		errorp("invalid storage class for function '%s'", name);
}

static Symbol *
redcl(Symbol *sym, Type *tp, Symbol **pars, int sclass)
{
	int flags;
	char *name = sym->name;

	if (!eqtype(sym->type, tp, 1)) {
		errorp("conflicting types for '%s'", name);
		return sym;
	}

	if (sym->token == TYPEIDEN && sclass != TYPEDEF ||
	    sym->token != TYPEIDEN && sclass == TYPEDEF) {
		goto redeclaration;
	}
	if (curctx != GLOBALCTX && tp->op != FTN) {
		/* is it the redeclaration of a local variable? */
		if ((sym->flags & SEXTERN) && sclass == EXTERN)
			return sym;
		goto redeclaration;
	}

	sym->u.pars = pars;

	flags = sym->flags;
	switch (sclass) {
	case REGISTER:
	case AUTO:
		bad_storage(tp, name);
		break;
	case NOSCLASS:
		if ((flags & SPRIVATE) == 0) {
			flags &= ~SEXTERN;
			flags |= SGLOBAL;
			break;
		}
		errorp("non-static declaration of '%s' follows static declaration",
		       name);
		break;
	case TYPEDEF:
	case EXTERN:
		break;
	case STATIC:
		if ((flags & (SGLOBAL|SEXTERN)) == 0) {
			flags |= SPRIVATE;
			break;
		}
		errorp("static declaration of '%s' follows non-static declaration",
		       name);
		break;
	}
	sym->flags = flags;

	return sym;

redeclaration:
	errorp("redeclaration of '%s'", name);
	return sym;
}

static Symbol *
identifier(struct decl *dcl)
{
	Symbol *sym = dcl->sym;
	Type *tp = dcl->type;
	int sclass = dcl->sclass;
	char *name = sym->name;

	if (empty(sym, tp, 0))
		return sym;

	/* TODO: Add warning about ANSI limits */
	if (!(tp->prop & TDEFINED)                &&
	    sclass != EXTERN && sclass != TYPEDEF &&
	    !(tp->op == ARY && yytoken == '=')) {
		errorp("declared variable '%s' of incomplete type", name);
	}

	if (tp->op != FTN) {
		sym = install(NS_IDEN, sym);
	} else {
		if (sclass == NOSCLASS)
			sclass = EXTERN;
		/*
		 * Ugly workaround to solve function declarations.
		 * A new context is added for the parameters,
		 * so at this point curctx is incremented by
		 * one since sym was parsed.
		 */
		--curctx;
		sym = install(NS_IDEN, sym);
		++curctx;
		if (!strcmp(name, "main") && tp->type != inttype) {
			errorp("main shall be defined with a return type of int");
			errorp("please contact __20h__ on irc.freenode.net (#bitreich-en) via IRC");
		}
	}

	if (sym == NULL) {
		sym = redcl(dcl->sym, tp, dcl->pars, sclass);
	} else {
		short flags = sym->flags;

		sym->type = tp;
		sym->u.pars = dcl->pars;

		switch (sclass) {
		case REGISTER:
		case AUTO:
			if (curctx != GLOBALCTX && tp->op != FTN) {
				flags |= (sclass == REGISTER) ? SREGISTER : SAUTO;
				break;
			}
			bad_storage(tp, name);
		case NOSCLASS:
			if (tp->op == FTN)
				flags |= SEXTERN;
			else
				flags |= (curctx == GLOBALCTX) ? SGLOBAL : SAUTO;
			break;
		case EXTERN:
			flags |= SEXTERN;
			break;
		case STATIC:
			flags |= (curctx == GLOBALCTX) ? SPRIVATE : SLOCAL;
			break;
		case TYPEDEF:
			flags |= STYPEDEF;
			sym->u.token = sym->token = TYPEIDEN;
			break;
		}
		sym->flags = flags;
	}

	if (accept('='))
		initializer(sym, sym->type);
	if (!(sym->flags & (SGLOBAL|SEXTERN)) && tp->op != FTN)
		sym->flags |= SDEFINED;
	if (sym->token == IDEN && sym->type->op != FTN)
		emit(ODECL, sym);
	return sym;
}

static Symbol *
dodcl(int rep, Symbol *(*fun)(struct decl *), unsigned ns, Type *parent)
{
	Symbol *sym;
	Type *base;
	struct decl dcl;
	struct declarators stack;

	dcl.ns = ns;
	dcl.parent = parent;
	base = specifier(&dcl.sclass, &dcl.qualifier);

	do {
		stack.nr = 0;
		dcl.pars = NULL;
		dcl.type = base;

		declarator(&stack, ns);

		while (pop(&stack, &dcl))
			/* nothing */;
		sym = (*fun)(&dcl);
	} while (rep && accept(','));

	return sym;
}

void
decl(void)
{
	Symbol **p, *par, *sym, *ocurfun;

	if (accept(';'))
		return;
	sym = dodcl(REP, identifier, NS_IDEN, NULL);

	if (sym->type->op != FTN) {
		expect(';');
		return;
	}

	ocurfun = curfun;
	curfun = sym;
	/*
	 * Functions only can appear at global context,
	 * but due to parameter context, we have to check
	 * against GLOBALCTX+1
	 */
	if (curctx != GLOBALCTX+1 || yytoken == ';') {
		emit(ODECL, sym);
		/*
		 * avoid non used warnings in prototypes
		 */
		for (p = sym->u.pars;  p && *p; ++p)
			(*p)->flags |= SUSED;
		popctx();
		expect(';');
		free(sym->u.pars);
		sym->u.pars = NULL;
		curfun = ocurfun;
		return;
	}
	if (sym->type->prop & TK_R) {
		while (yytoken != '{') {
			par = dodcl(REP, parameter, NS_IDEN, sym->type);
			expect(';');
		}
	}

	if (sym->flags & STYPEDEF)
		errorp("function definition declared 'typedef'");
	if (sym->flags & SDEFINED)
		errorp("redefinition of '%s'", sym->name);
	if (sym->flags & SEXTERN) {
		sym->flags &= ~SEXTERN;
		sym->flags |= SGLOBAL;
	}

	sym->flags |= SDEFINED;
	sym->flags &= ~SEMITTED;
	emit(OFUN, sym);
	compound(NULL, NULL, NULL);
	emit(OEFUN, NULL);
	flushtypes();
	curfun = ocurfun;
}

static void
fieldlist(Type *tp)
{
	if (yytoken != ';')
		dodcl(REP, field, tp->ns, tp);
	expect(';');
}

Type *
typename(void)
{
	return dodcl(NOREP, type, NS_DUMMY, NULL)->type;
}

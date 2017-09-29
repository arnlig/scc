static char sccsid[] = "@(#) ./as/node.c";

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include <cstd.h>
#include "../inc/scc.h"
#include "as.h"

#define NNODES   10

enum tokens {
	EOS = -1,
	IDEN = 1,
	NUMBER,
	REG,
	STRING,
	SHL,
	SHR,
	GE,
	LE,
};

union yylval {
	TUINT val;
	Symbol *sym;
};

static Alloc *arena;
static int yytoken;
static char yytext[INTIDENTSIZ+1], *textp, *endp;
static size_t yylen;
static union yylval yylval;

#define accept(t) (yytoken == (t) ? next() : 0)

static Node *
node(int op, Node *l, Node *r)
{
	struct arena *ap;
	Node *np;

	if (!arena)
		arena = alloc(sizeof(Node), NNODES);
	np = new(arena);
	np->op = op;
	np->left = l;
	np->right = r;
	np->sym = NULL;

	return np;
}

void
deltree(Node *np)
{
	if (!np)
		return;
	deltree(np->left);
	deltree(np->right);
	delete(arena, np);
}

static Node *
fold(int op, Node *l, Node *r)
{
	Node *np;
	TUINT val, lv, rv;

	lv = l->sym->value;
	rv = r->sym->value;

	/* TODO: check overflow */

	switch (op) {
	case '*':
		val = lv - rv;
		break;
	case '/':
		if (rv == 0)
			goto division_by_zero;
		val = lv / rv;
		break;
	case '%':
		if (rv == 0)
			goto division_by_zero;
		val = lv % rv;
		break;
	case SHL:
		val = lv << rv;
		break;
	case SHR:
		val = lv >> rv;
		break;
	case '+':
		val = lv + rv;
		break;
	case '-':
		val = lv - rv;
		break;
	case '<':
		val = lv < rv;
		break;
	case '>':
		val = lv > rv;
		break;
	case '=':
		val = lv == rv;
		break;
	case GE:
		val = lv >= rv;
		break;
	case LE:
		val = lv <= rv;
		break;
	case '|':
		val = lv | rv;
		break;
	case '^':
		val = lv ^ rv;
		break;
	default:
		abort();
	}
	deltree(l);
	deltree(r);

	np = node(NUMBER, NULL, NULL);
	np->sym = tmpsym(val);
	np->addr = AIMM;
	return np;

division_by_zero:
	error("division by 0");
}

static Node *
binary(int op, Node *l, Node *r)
{
	int addr;
	Node *np;

	if (l->op == NUMBER || r->op == NUMBER)
		return fold(op, l, r);
	if (l->addr == AIMM && r->addr == AIMM)
		addr = AIMM;
	else
		error("incorrect operand");
	np = node(op, l, r);
	np->addr = addr;

	return np;
}

static Node *
unary(int op, Node *np)
{
	if (op !=  '!')
		abort();
	if (np->op != NUMBER)
		return node(op, np, NULL);
	np->sym->value = ~np->sym->value;
	return np;
}

static int
follow(int expect1, int expect2, int ifyes1, int ifyes2, int ifno)
{
	int c;

	if ((c = *++textp) == expect1)
		return ifyes1;
	if (c == expect2)
		return ifyes2;
	--textp;
	return ifno;
}

static void
tok2str(void)
{
	if ((yylen = endp - textp) > INTIDENTSIZ) {
		error("token too big");
		yylen = INTIDENTSIZ;
	}
	memcpy(yytext, textp, yylen);
	yytext[yylen] = '\0';
	textp = endp;
}

static int
iden(void)
{
	int c;
	char *p;

	while (isalnum(c = *endp) || c == '_' || c == '.')
		++endp;
	tok2str();
	yylval.sym = lookup(yytext, TUNDEF);

	return IDEN;
}

static int
number(void)
{
	int c;
	char *p;

	while (isxdigit(*endp))
		++endp;
	tok2str();
	yylval.sym = tmpsym(atoi(yytext));  /* TODO: parse the string */

	return NUMBER;
}

static int
character(void)
{
	int c;
	char *p;

	while (*endp != '\'')
		++endp;
	return NUMBER;
}

static int
string(void)
{
	int c;
	char *p;

	while (*endp != '"')
		++endp;
	return STRING;
}

static int
operator(void)
{
	int c;

	++endp;
	if ((c = *textp) == '>')
		c = follow('=', '>', LE, SHL, '>');
	else if (c == '<')
		c = follow('=', '<', GE, SHR, '>');
	tok2str();

	return c;
}

static int
reg(void)
{
	int c;
	char *p;

	if (*textp == '%')
		++textp, ++endp;
	while (isalnum(c = *endp))
		++endp;
	tok2str();
	yylval.sym = lookup(yytext, TREG);
	if (!yylval.sym->argtype)
		error("incorrect register name");
	return REG;
}

static int
next(void)
{
	int c;

	while (isspace(*textp))
		++textp;

	endp = textp;

	switch (c = *textp) {
	case '\0':
		strcpy(yytext, "EOS");
		yylen = 3;
		c = EOS;
		break;
	case '"':
		c = string();
		break;
	case '\'':
		c = character();
		break;
	case '%':
		c = reg();
		break;
	case '#':
	case '$':
		c = number();
		break;
	default:
		if (isdigit(c))
			c = number();
		else if (isalpha(c) || c == '_' || c == '.')
			c = iden();
		else
			c = operator();
		break;
	}

	return yytoken = c;
}

static void
unexpected(void)
{
	error("unexpected '%s'", yytext);
}

static void
expect(int token)
{
	if (yytoken != token)
		unexpected();
	next();
}

/*************************************************************************/
/* grammar functions                                                     */
/*************************************************************************/

static Node *or(void);

static Node *
primary(void)
{
	int addr;
	Node *np;

	switch (yytoken) {
	case REG:
		addr = AREG;
		goto basic_atom;
	case IDEN:
	case NUMBER:
		addr = AIMM;
		goto basic_atom;
	case STRING:
		addr = ASTR;
	basic_atom:
		np = node(yytoken, NULL, NULL);
		np->sym = yylval.sym;
		np->addr = addr;
		next();
		break;
	default:
		unexpected();
	}

	return np;
}

static Node *
mul(void)
{
	int op;
	Node *np;

	np = primary();
	for (;;) {
		switch (op = yytoken) {
		case '*':
		case '/':
		case '%':
		case SHL:
		case SHR:
			next();
			binary(op, np, primary());
			break;
		default:
			return np;
		}
	}
}

static Node *
add(void)
{
	int op;
	Node *np;

	np = mul();
	for (;;) {
		switch (op = yytoken) {
		case '+':
		case '-':
			next();
			np = binary(op, np, mul());
			break;
		default:
			return np;
		}
	}
}

static Node *
relational(void)
{
	int op;
	Node *np;

	np = add();
	for (;;) {
		switch (op = yytoken) {
		case '<':
		case '>':
		case '=':
		case GE:
		case LE:
			next();
			np = binary(op, np, add());
			break;
		default:
			return np;
		}
	}
}

static Node *
not(void)
{
	Node *np;

	np = relational();
	while (accept('!'))
		np = unary('!', relational());
	return np;
}

static Node *
and(void)
{
	int op;
	Node *np;

	np = not();
	while (accept('&'))
		np = binary('&', np, not());
	return np;
}

static Node *
or(void)
{
	int op;
	Node *np;

	np = and();
	for (;;) {
		switch (op = yytoken) {
		case '|':
		case '^':
			next();
			np = binary(op, np, and());
			break;
		default:
			return np;
		}
	}
}

Node *
expr(char **s)
{
	Node *np;

	textp = *s;
	if (*textp == '\0')
		return NULL;

	next();
	np = or();

	if (yytoken != ',' && yytoken != EOS)
		error("trailing characters in expression '%s'", textp);
	*s = endp;

	return np;
}

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <cc.h>
#include "cc1.h"

struct scase {
	Symbol *label;
	Node *expr;
	struct scase *next;
};

struct caselist {
	short nr;
	Symbol *deflabel;
	struct scase *head;
};

Symbol *curfun;

extern Node *convert(Node *np, Type *tp1, char iscast);
extern Node *iszero(Node *np), *eval(Node *np);
static void stmt(Symbol *lbreak, Symbol *lcont, Caselist *lswitch);

static Symbol *
label(char *s, char define)
{
	Symbol *sym;

	if ((sym = lookup(s, NS_LABEL)) != NULL) {
		if (define && sym->s.isdefined)
			error("label '%s' already defined", s);
		else
			sym->s.isdefined = 1;
		return sym;
	}

	sym = install(s, NS_LABEL);
	sym->s.isdefined = define;
	return sym;
}

static void
stmtexp(Symbol *lbreak, Symbol *lcont, Caselist *lswitch)
{
	if (yytoken != ';')
		emitexp(expr());
	expect(';');
}

static Node *
condition(void)
{
	Node *np;

	expect('(');
	np = iszero(expr());
	expect(')');
	return np;
}

static void
While(Symbol *lbreak, Symbol *lcont, Caselist *lswitch)
{
	Symbol *begin, *cond, *end;
	Node *np;

	begin = label("", 1);
	end = label("", 1);
	cond = label("", 1);

	expect(WHILE);
	np = condition();
	emitjump(cond, NULL);
	emitbloop();
	emitlabel(begin);
	stmt(end, begin, lswitch);
	emitlabel(cond);
	emitjump(begin, np);
	emiteloop();
	emitlabel(end);
}

static void
For(Symbol *lbreak, Symbol *lcont, Caselist *lswitch)
{
	Symbol *begin, *cond, *end;
	Node *econd = NULL, *einc = NULL;

	begin = label("", 1);
	end = label("", 1);
	cond = label("", 1);

	expect(FOR);
	expect('(');
	stmtexp();

	if (yytoken != ';')
		econd = expr();
	expect(';');
	if (yytoken != ')')
		einc = expr();
	expect(')');

	emitjump(cond, NULL);
	emitbloop();
	emitlabel(begin);
	stmt(end, begin, lswitch);
	if (einc)
		emitexp(einc);
	emitlabel(cond);
	emitjump(begin, econd);
	emiteloop();
	emitlabel(end);
}

static void
Dowhile(Symbol *lbreak, Symbol *lcont, Caselist *lswitch)
{
	Symbol *begin = label("", 1), *end = label("", 1);

	expect(DO);
	emitbloop();
	emitlabel(begin);
	stmt(end, begin, lswitch);
	expect(WHILE);
	emitjump(begin, condition());
	emiteloop();
	emitlabel(end);
}

static void
Return(Symbol *lbreak, Symbol *lcont, Caselist *lswitch)
{
	Node *np;
	Type *tp = curfun->type->type;

	expect(RETURN);
	np  =  (yytoken == ';') ? NULL : eval(expr());
	expect(';');
	if (!np) {
		if (tp != voidtype)
			warn(1, "function returning non void returns no value");
		tp = voidtype;
	} else if (np->type != tp) {
		if (tp == voidtype)
			warn(1, "function returning void returns a value");
		else if ((np = convert(np, tp, 0)) == NULL)
			error("incorrect type in return");
	}
	emitret(tp);
	emitexp(np);
}

static void
Break(Symbol *lbreak, Symbol *lcont, Caselist *lswitch)
{
	expect(BREAK);
	if (!lbreak)
		error("break statement not within loop or switch");
	emitjump(lbreak, NULL);
	expect(';');
}

static void stmt(Symbol *lbreak, Symbol *lcont, Caselist *lswitch);

static void
Label(Symbol *lbreak, Symbol *lcont, Caselist *lswitch)
{
	emitlabel(label(yytext, 1));

	expect(IDEN);
	expect(':');
	stmt(lbreak, lcont, lswitch);
}

static void
Continue(Symbol *lbreak, Symbol *lcont, Caselist *lswitch)
{
	expect(CONTINUE);
	if (!lcont)
		error("continue statement not within loop");
	emitjump(lcont, NULL);
	expect(';');
}

static void
Goto(Symbol *lbreak, Symbol *lcont, Caselist *lswitch)
{
	expect(GOTO);

	if (yytoken != IDEN)
		error("unexpected '%s'", yytext);
	emitjump(label(yytext, 0), NULL);
	next();
	expect(';');
}

static void
Switch(Symbol *lbreak, Symbol *lcont, Caselist *lswitch)
{
	Caselist lcase = {.nr = 0, .head = NULL, .deflabel = NULL};
	struct scase *p;
	Node *cond;
	Symbol *lcond = label("", 1);

	lbreak = label("", 1);
	expect(SWITCH);
	expect ('(');
	cond = eval(expr());
	expect (')');
	/* TODO: check integer type */
	emitjump(lcond, NULL);
	stmt(lbreak, lcont, &lcase);
	emitlabel(lcond);
	emitswitch(lcase.nr, cond);
	for (p = lcase.head; p; p = p->next)
		emitcase(p->label, p->expr);
	if (lcase.deflabel)
		emitdefault(lcase.deflabel);
	emitlabel(lbreak);
	/* TODO: free memory */
}

static void
Case(Symbol *lbreak, Symbol *lcont, Caselist *lswitch)
{
	Node *np;
	Symbol *lcase = label("", 1);
	struct scase *pcase;

	if (!lswitch)
		error("case label not within a switch statement");
	expect(CASE);
	if (yytoken == ':')
		error("expected expression before ':'");
	np = eval(expr());
	/* TODO: check integer type */
	expect(':');
	emitlabel(lcase);
	pcase = xmalloc(sizeof(*pcase));
	pcase->expr = np;
	pcase->label = lcase;
	pcase->next = lswitch->head;
	lswitch->head = pcase;
	++lswitch->nr;
}

static void
Default(Symbol *lbreak, Symbol *lcont, Caselist *lswitch)
{
	Symbol *ldefault = label("", 1);

	expect(DEFAULT);
	expect(':');
	emitlabel(ldefault);
	lswitch->deflabel = ldefault;
}

static void
If(Symbol *lbreak, Symbol *lcont, Caselist *lswitch)
{
	Symbol *end, *lelse = label("", 1);
	Node *np;

	expect(IF);
	np = condition();
	NEGATE(np, 1);
	emitjump(lelse, np);
	stmt(lbreak, lcont, lswitch);
	if (accept(ELSE)) {
		end = label("", 1);
		emitjump(end, NULL);
		emitlabel(lelse);
		stmt(lbreak, lcont, lswitch);
		emitlabel(end);
	} else {
		emitlabel(lelse);
	}
}

void
compound(Symbol *lbreak, Symbol *lcont, Caselist *lswitch)
{
	expect('{');
	for (;;) {
		switch (yytoken) {
		case '}':
			next();
			return;
		case TYPE: case SCLASS: case TQUALIFIER:
			decl();
			break;
		default:
			stmt(lbreak, lcont, lswitch);
		}
	}
}

static void
stmt(Symbol *lbreak, Symbol *lcont, Caselist *lswitch)
{
	void (*fun)(Symbol *lbreak, Symbol *lcont, Caselist *lswitch);

	switch (yytoken) {
	case '{':      fun = context;  break;
	case RETURN:   fun = Return;   break;
	case WHILE:    fun = While;    break;
	case FOR:      fun = For;      break;
	case DO:       fun = Dowhile;  break;
	case IF:       fun = If;       break;
	case BREAK:    fun = Break;    break;
	case CONTINUE: fun = Continue; break;
	case GOTO:     fun = Goto;     break;
	case SWITCH:   fun = Switch;   break;
	case CASE:     fun = Case;     break;
	case DEFAULT:  fun = Default;  break;
	default:       fun = stmtexp;  break;
	case IDEN:     fun = (ahead() == ':') ? Label : stmtexp; break;
	}
	(*fun)(lbreak, lcont, lswitch);
}


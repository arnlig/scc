
#include <stddef.h>
#include <stdint.h>

#include "cc.h"

Symbol *curfun;

extern void decl(void);
extern Node *expr(void);
extern Node *convert(Node *np, Type *tp1, char iscast);

static void
Return(void)
{
	Node *np;
	Type *tp = curfun->type->type;

	expect(RETURN);
	np = expr();
	if (np->type != tp) {
		if ((np = convert(np, tp, 0)) == NULL)
			error("incorrect type in return");
	}
	emitret(tp);
	emitexp(np);
}

void
compound(void)
{
	expect('{');
	while (!accept('}')) {
		switch (yytoken) {
		case TYPE: case SCLASS: case TQUALIFIER:
			decl();
			break;
		case RETURN:
			Return();
			break;
		default:
			emitexp(expr());
		}
		expect(';');
	}
}

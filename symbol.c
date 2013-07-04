
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "cc.h"
#include "symbol.h"

#define NR_SYM_HASH 32

unsigned char curctx;

static struct symbol *htab[NR_SYM_HASH];
static struct symbol *head, *headfun;


static inline unsigned char
hash(register const char *s)
{
	register unsigned char h, ch;

	for (h = 0; ch = *s++; h += ch)
		/* nothing */;
	return h & NR_SYM_HASH - 1;
}

void
new_ctx(void)
{
	++curctx;
}

void
del_ctx(void)
{
	register struct symbol *sym, *next;
	static char *s;

	for (sym = head; sym; sym = next) {
		if (sym->ctx <= curctx)
			break;
		if ((s = sym->name) != NULL)
			htab[hash(s)] = sym->hash;
		next = sym->next;
		sym->next = headfun;
		headfun = sym;
	}
	--curctx;
}

void
freesyms(void)
{
	register struct symbol *sym, *next;

	if (curctx == CTX_OUTER) {
		for (sym = headfun; sym; sym = next) {
			next = sym->next;
			free(sym->name);
			free(sym);
		}
	}
}

struct symbol *
find(const char *s, register char ns)
{
	register struct symbol *sym;
	static unsigned char l;

	l = strlen(s);
	for (sym = htab[hash(s)]; sym; sym = sym->hash) {
		if (ns != NS_ANY && ns != sym->ns)
			continue;
		if (!memcmp(sym->name, s, l))
			return sym;
	}
	return NULL;
}

struct symbol *
lookup(register const char *s, char ns)
{
	register struct symbol *sym;
	register unsigned char l;
	static unsigned char key;

	l = strlen(s);
	key = hash(s);
	for (sym = htab[key]; sym; sym = sym->hash) {
		if (ns != NS_ANY && ns != sym->ns)
			continue;
		if (!memcmp(sym->name, s, l))
			return sym;
	}
	sym = xmalloc(sizeof(*sym));
	sym->name = xstrdup(s);
	sym->next = head;
	sym->ctx = curctx;
	sym->ns = ns;
	head = sym;
	sym->hash = htab[key];
	htab[key] = sym;

	return sym;
}

void
insert(struct symbol *sym, unsigned char ctx)
{
	register struct symbol *p, *q;

	for (q = p = head; p; q = p, p = p->next) {
		if (p == sym)
			break;
	}
	assert(p);			/* sym must be in the list */
	q->next = p->next;		/* remove from the list */

	for (q = p = head; p; q = p, p = p->next) {
		if (p->ctx <= ctx)
			break;
	}
	if (q == NULL) {
		head = sym;
		sym->next = NULL;
	} else {
		q->next = sym;
		sym->next = p;
	}
}

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include <scc/mach.h>

#include "ld.h"

static void
rebase(Obj *obj)
{
	Symbol *aux;
	Objsym *sym;

	for (sym = obj->syms; sym; sym = sym->next) {
		switch (sym->type) {
		case 'T':
		case 'D':
		case 'B':
			/*
			 * this lookup must succeed, otherwise
			 * we have an error in our code.
			 */
			aux = lookup(sym->name);
			aux->value += obj->secs[sym->sect].base;
		case 't':
		case 'd':
		case 'b':
			sym->value += obj->secs[sym->sect].base;
		case 'U':
		case 'N':
		case '?':
			break;
		default:
			abort();
		}
	}
}

/*
 * rebase all the sections
 */
void
pass3(int argc, char *argv[])
{
	Obj *obj;
	Objlst *lst;
	Objsect *sp;
	Segment *seg;

	/*
	 * TODO: deal with page aligment
	 */
	text.base = 0x100;
	rodata.base = text.base + text.size;
	data.base = rodata.base + rodata.size;
	bss.base = data.base + data.size;

	for (lst = objhead; lst; lst = lst->next) {
		obj = lst->obj;
		for (sp = obj->secs; sp; sp = sp->next) {
			switch (sp->type) {
			case 'T':
				seg = &text;
				break;
			/* TODO: what happens with rodata? */
			case 'D':
				seg = &data;
				break;
			case 'B':
				seg = &bss;
				break;
			default:
				abort();
			}
			sp->base = seg->base + seg->size;
			/* TODO: deal with symbol aligment */
			seg->size += sp->size;
		}
		rebase(obj);
	}
}
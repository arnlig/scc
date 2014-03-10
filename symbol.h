
#pragma once
#ifndef SYMBOL_H
#define SYMBOL_H

#if ! __bool_true_false_are_defined
# include <stdbool.h>
#endif

#define CTX_OUTER 0
#define CTX_FUNC  1

enum {
	NS_IDEN = 0,
	NS_TYPE,
	NS_KEYWORD,
	NS_LABEL,
	NS_TAG
};

struct qualifier {
	bool c_const : 1;
	bool c_volatile : 1;
	bool defined: 1;
};

struct storage {
	bool c_typedef : 1;
	bool c_extern : 1;
	bool c_static : 1;
	bool c_auto : 1;
	bool c_register : 1;
	bool defined: 1;
};

struct ctype {
	unsigned type : 5;
	bool c_const : 1;
	bool c_restrict : 1;
	bool c_volatile : 1;
	bool c_unsigned : 1;
	bool c_signed : 1;
	bool forward : 1;
	bool defined: 1;
	union {
		struct {
			unsigned char ns;
			char *tag;
		};
		unsigned len;
	};
	struct ctype *base;
};

struct symbol {
	struct ctype ctype;
	struct storage store;
	struct qualifier qlf;
	unsigned char ctx;
	unsigned char ns;
	char *name;
	unsigned char tok;
	struct {
		union {
			char c;   /* numerical constants */
			short s;
			int i;
			long *l;
			long long *ll;
			unsigned char label;
		};
	};
	struct symbol *next;
	struct symbol *hash;
};


extern struct ctype *decl_type(struct ctype *t);
extern void pushtype(unsigned mod);
extern struct ctype *ctype(struct ctype *tp, unsigned char tok);
extern void new_ctx(void);
extern void del_ctx(void);
extern void freesyms(void);
extern struct symbol *lookup(const char *s, signed char ns);
extern void insert(struct symbol *sym, unsigned char ctx);
extern struct storage *storage(struct storage *tp, unsigned char mod);
extern struct qualifier *qualifier(register struct qualifier *, unsigned char);
extern void delctype(struct ctype *tp);
extern unsigned char hash(register const char *s);
extern struct ctype *initctype(register struct ctype *tp);
extern struct storage *initstore(register struct storage *store);
extern struct qualifier * initqlf(struct qualifier *qlf);

#ifndef NDEBUG
extern void ptype(register struct ctype *t);
#else
#  define ptype(t)
#endif

#endif

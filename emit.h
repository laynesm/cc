#ifndef EMIT_H
#define EMIT_H

#include "cc.h"

#define NONE  0
#define REGIS 1
#define STACK 2

struct lval {
	int          lval_off;
	int          lval_kind;
	struct symty lval_ty;
};

void regispre(void);
void regispost(void);

void deptr(struct lval);
void ptr(struct lval);

void cmp(long);
void jmplbl(int);
void jnelbl(int);
void jelbl(int);
void idlbl(int);
void jmp(char *);
void prologue(void);
void epilogue(void);
void load(struct lval);
void store(struct lval);
void eq(struct lval);
void ne(struct lval);
void lt(struct lval);
void gt(struct lval);
void le(struct lval);
void ge(struct lval);
void neg(struct lval);
void lbl(const char *);
void globl(const char *);
void retval(long);
void ret(void);
void add(struct lval);
void sub(struct lval);
void idiv(struct lval);
void mul(struct lval);
void rem(struct lval);
void bnot(struct lval);
void lnot(struct lval);
void pos(struct lval);

#endif

#ifndef EMIT_H
#define EMIT_H

#include "cc.h"

#define NONE  0
#define REGIS 1
#define STACK 2

struct lval {
	int           lval_off;
	int           lval_kind;
	struct symty *lval_ty;
};

extern struct lval lval;

void regispre(void);
void regispost(void);

void deptr(struct lval);
void ptr(struct lval);

void inc(struct lval);
void dec(struct lval);

void and(struct lval);
void or(struct lval);

void band(struct lval);
void bor(struct lval);
void bxor(struct lval);
void bshl(struct lval);
void bshr(struct lval);

void bandeq(struct lval);
void boreq(struct lval);
void bxoreq(struct lval);
void bshleq(struct lval);
void bshreq(struct lval);

void preinc(struct lval);
void predec(struct lval);

void addeq(struct lval);
void subeq(struct lval);
void muleq(struct lval);
void diveq(struct lval);
void remeq(struct lval);

void idx(struct lval);
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
void retval(unsigned long long);
void ret(void);
void add(struct lval);
void sub(struct lval);
void idiv(struct lval);
void mul(struct lval);
void rem(struct lval);
void bnot(struct lval);
void lnot(struct lval);
void pos(struct lval);
void cast(struct symty *);

#endif

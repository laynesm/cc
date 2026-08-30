#ifndef EMIT_H
#define EMIT_H

#include "cc.h"

#define NONE  0
#define REGIS 1
#define STACK 2

struct lval {
	int lval_off;
	int lval_kind;
	struct symty lval_ty;
};

void emit_regis_preamble(void);
void emit_regis_postamble(void);

void emit_deptr(struct lval);
void emit_ptr(struct lval);

void emit_cmp(long);
void emit_jmp_label(int);
void emit_jne_label(int);
void emit_je_label(int);
void emit_label_id(int);
void emit_jmp(char *);
void emit_prologue(void);
void emit_epilogue(void);
void emit_load_var(struct lval);
void emit_store_var(struct lval);
void emit_eq(struct lval);
void emit_ne(struct lval);
void emit_lt(struct lval);
void emit_gt(struct lval);
void emit_le(struct lval);
void emit_ge(struct lval);
void emit_neg(struct lval);
void emit_label(const char *);
void emit_globl(const char *);
void emit_retval(long);
void emit_ret(void);
void emit_add(struct lval);
void emit_sub(struct lval);
void emit_div(struct lval);
void emit_mul(struct lval);
void emit_rem(struct lval);
void emit_bnot(struct lval);
void emit_lnot(struct lval);
void emit_pos(struct lval);

#endif

#include <stdio.h>

#include "parse.h"
#include "emit.h"
#include "util.h"

void emit_regis_preamble(void)
{
	printf("	push %%rax\n");
}

void emit_regis_postamble(void)
{
	printf("	pop %%rcx\n");
}

void emit_deptr(struct lval)
{
	printf("	mov (%%rax), %%rax\n");
}

void emit_ptr(struct lval l)
{
	printf("	lea %d(%%rbp), %%rax\n", l.lval_off);
}

void emit_jmp_label(int id)
{
	printf("	jmp .L%d\n", id);
}

void emit_jne_label(int id)
{
	printf("	jne .L%d\n", id);
}

void emit_je_label(int id)
{
	printf("	je .L%d\n", id);
}

void emit_label_id(int id)
{
	printf(".L%d:\n", id);
}

void emit_cmp(long val)
{
	printf("	cmp $%ld, %%rax\n", val);
}

void emit_jmp(char *label)
{
	printf("	jmp %s\n", label);
}

void emit_prologue(void)
{
	printf("	push %%rbp\n");
	printf("	mov %%rsp, %%rbp\n");
	printf("	sub $256, %%rsp\n");
}

void emit_epilogue(void)
{
	printf("	mov %%rbp, %%rsp\n");
	printf("	pop %%rbp\n");
}

void emit_load_var(struct lval l)
{
	if (l.lval_kind == REGIS) {
		printf("	mov (%%rax),%%rax\n");
		return;
	}

	printf("	mov %d(%%rbp), %%rax\n", l.lval_off);
}

void emit_store_var(struct lval l)
{
	if (l.lval_kind == REGIS) {
		printf("	mov %%rax, (%%rcx)\n");
		return;
	}

	printf("	mov %%rax, %d(%%rbp)\n", l.lval_off);
}

void emit_label(const char *s)
{
	printf("%s:\n", s);
}

void emit_globl(const char *s)
{
	printf(".globl %s\n", s);
	emit_label(s);
}

void emit_eq(struct lval)
{
	printf("	cmp %%rcx, %%rax\n	sete %%al\n	movzbq %%al, "
	       "%%rax\n");
}

void emit_ne(struct lval)
{
	printf("	cmp %%rcx, %%rax\n	setne %%al\n	movzbq %%al, "
	       "%%rax\n");
}

void emit_lt(struct lval)
{
	printf("	cmp %%rcx, %%rax\n	setl %%al\n	movzbq %%al, "
	       "%%rax\n");
}

void emit_gt(struct lval)
{
	printf("	cmp %%rcx, %%rax\n	setg %%al\n	movzbq %%al, "
	       "%%rax\n");
}

void emit_le(struct lval)
{
	printf("	cmp %%rcx, %%rax\n	setle %%al\n	movzbq %%al, "
	       "%%rax\n");
}

void emit_ge(struct lval)
{
	printf("	cmp %%rcx, %%rax\n	setge %%al\n	movzbq %%al, "
	       "%%rax\n");
}

void emit_pos(struct lval) {}

void emit_bnot(struct lval)
{
	printf("	not %%rax\n");
}

void emit_lnot(struct lval)
{
	printf("	cmp $0, %%rax\n");
	printf("	sete %%al\n");
	printf("	movzbq %%al, %%rax\n");
}

void emit_neg(struct lval)
{
	printf("	neg %%rax\n");
}

void emit_retval(long val)
{
	printf("	mov $%ld,%%rax\n", val);
}

void emit_ret(void)
{
	printf("	ret\n");
}

void emit_add(struct lval)
{
	printf("	add %%rcx, %%rax\n");
}

void emit_sub(struct lval)
{
	printf("	sub %%rcx, %%rax\n");
}

void emit_mul(struct lval)
{
	printf("	imul %%rcx, %%rax\n");
}

void emit_div(struct lval)
{
	printf("	cqo\n	idiv %%rcx\n");
}

void emit_rem(struct lval)
{
	printf("	cqo\n	idiv %%rcx\n	mov %%rdx, %%rax\n");
}

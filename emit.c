#include <stdio.h>

#include "emit.h"
#include "parse.h"
#include "util.h"

static void load_mem(int off, const char *from, int size, int sign)
{
	switch (size) {
	case 1:
		printf("mov%cbq %d(%s), %%rax\n", sign ? 's' : 'z', off, from);
		break;
	case 2:
		printf("mov%cwq %d(%s), %%rax\n", sign ? 's' : 'z', off, from);
		break;
	case 4:
		printf("	%s %d(%s), %%rax\n", sign ? "movslq" : "movl",
		       off, from);
		break;
	case 8:
		printf("	mov %d(%s), %%rax\n", off, from);
		break;
	default:
		error("not yet implemented - load");
	}
}

static void store_mem(int off, const char *to, int size)
{
	switch (size) {
	case 1:
		printf("	movb %%al, %d(%s)\n", off, to);
		break;
	case 2:
		printf("	movw %%ax, %d(%s)\n", off, to);
		break;
	case 4:
		printf("	movl %%eax, %d(%s)\n", off, to);
		break;
	case 8:
		printf("	movq %%rax, %d(%s)\n", off, to);
		break;
	default:
		error("not implemented yet - store");
	}
}

void regispre(void)
{
	printf("	push %%rax\n");
}

void regispost(void)
{
	printf("	pop %%rcx\n");
}

void deptr(struct lval l)
{
	load_mem(0, "%rax", l.lval_ty.sty_size, l.lval_ty.sty_signed);
}

void ptr(struct lval l)
{
	printf("	lea %d(%%rbp), %%rax\n", l.lval_off);
}

void jmplbl(int id)
{
	printf("	jmp .L%d\n", id);
}

void jnelbl(int id)
{
	printf("	jne .L%d\n", id);
}

void jelbl(int id)
{
	printf("	je .L%d\n", id);
}

void idlbl(int id)
{
	printf(".L%d:\n", id);
}

void cmp(long val)
{
	printf("	cmp $%ld, %%rax\n", val);
}

void jmp(char *label)
{
	printf("	jmp %s\n", label);
}

void prologue(void)
{
	printf("	push %%rbp\n");
	printf("	mov %%rsp, %%rbp\n");
	printf("	sub $256, %%rsp\n");
}

void epilogue(void)
{
	printf("	mov %%rbp, %%rsp\n");
	printf("	pop %%rbp\n");
}

void load(struct lval l)
{
	int size = l.lval_ty.sty_isptr ? 8 : l.lval_ty.sty_size;
	int sign = l.lval_ty.sty_signed;

	if (l.lval_kind == REGIS) {
		load_mem(0, "%rax", size, sign);
		return;
	}

	load_mem(l.lval_off, "%rbp", size, sign);
}

void store(struct lval l)
{
	int size = l.lval_ty.sty_isptr ? 8 : l.lval_ty.sty_size;

	if (l.lval_kind == REGIS) {
		store_mem(0, "%rcx", size);
		return;
	}

	store_mem(l.lval_off, "%rbp", size);
}

void lbl(const char *s)
{
	printf("%s:\n", s);
}

void globl(const char *s)
{
	printf(".globl %s\n", s);
	lbl(s);
}

void eq(struct lval)
{
	printf("	cmp %%rcx, %%rax\n	sete %%al\n	movzbq %%al, "
	       "%%rax\n");
}

void ne(struct lval)
{
	printf("	cmp %%rcx, %%rax\n	setne %%al\n	movzbq %%al, "
	       "%%rax\n");
}

void lt(struct lval)
{
	printf("	cmp %%rcx, %%rax\n	setl %%al\n	movzbq %%al, "
	       "%%rax\n");
}

void gt(struct lval)
{
	printf("	cmp %%rcx, %%rax\n	setg %%al\n	movzbq %%al, "
	       "%%rax\n");
}

void le(struct lval)
{
	printf("	cmp %%rcx, %%rax\n	setle %%al\n	movzbq %%al, "
	       "%%rax\n");
}

void ge(struct lval)
{
	printf("	cmp %%rcx, %%rax\n	setge %%al\n	movzbq %%al, "
	       "%%rax\n");
}

void pos(struct lval) {}

void bnot(struct lval)
{
	printf("	not %%rax\n");
}

void lnot(struct lval)
{
	printf("	cmp $0, %%rax\n");
	printf("	sete %%al\n");
	printf("	movzbq %%al, %%rax\n");
}

void neg(struct lval)
{
	printf("	neg %%rax\n");
}

void retval(long val)
{
	printf("	mov $%ld,%%rax\n", val);
}

void ret(void)
{
	printf("	ret\n");
}

void add(struct lval)
{
	printf("	add %%rcx, %%rax\n");
}

void sub(struct lval)
{
	printf("	sub %%rcx, %%rax\n");
}

void mul(struct lval)
{
	printf("	imul %%rcx, %%rax\n");
}

void idiv(struct lval)
{
	printf("	cqo\n	idiv %%rcx\n");
}

void rem(struct lval)
{
	printf("	cqo\n	idiv %%rcx\n	mov %%rdx, %%rax\n");
}

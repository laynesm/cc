#include <stdio.h>

#include "emit.h"
#include "parse.h"
#include "util.h"

static void emit_load(int off, const char *from, int size, int sign)
{
	switch (size) {
	case 1:
		printf("mov%cbq %d(%s), %%rax\n", sign ? 's' : 'z', off, from);
		break;
	case 2:
		printf("mov%cwq %d(%s), %%rax\n", sign ? 's' : 'z', off, from);
		break;
	case 4:
		printf("	%s %d(%s), %%rax\n", sign ? "movslq" : "movl", off, from);
		break;
	case 8:
		printf("	mov %d(%s), %%rax\n", off, from);
		break;
	default:
		error("not yet implemented - emit_load");
	}
}

static void emit_store(int off, const char *to, int size)
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
		error("not implemented yet - emit_store");
	}
}

void emit_regis_preamble(void)
{
	printf("	push %%rax\n");
}

void emit_regis_postamble(void)
{
	printf("	pop %%rcx\n");
}

void emit_deptr(struct lval l)
{
	emit_load(0, "%rax", l.lval_ty.sty_size, l.lval_ty.sty_signed);
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
	int size = l.lval_ty.sty_isptr ? 8 : l.lval_ty.sty_size;
	int sign = l.lval_ty.sty_signed;

	if (l.lval_kind == REGIS) {
		emit_load(0, "%rax", size, sign);
		return;
	}

	emit_load(l.lval_off, "%rbp", size, sign);
}

void emit_store_var(struct lval l)
{
	    int size = l.lval_ty.sty_isptr ? 8 : l.lval_ty.sty_size;

	        if (l.lval_kind == REGIS) {
			        emit_store(0, "%rcx", size);
				        return;
					    }

	    emit_store(l.lval_off, "%rbp", size);
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

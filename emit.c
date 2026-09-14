#include <stdio.h>
#include <string.h>

#include "emit.h"
#include "parse.h"
#include "util.h"

static void load_mem(int off, const char *from, int size, int sign)
{
	switch (size) {
	case 1:
		printf("	mov%cbq %d(%s), %%rax\n", sign ? 's' : 'z', off, from);
		break;
	case 2:
		printf("	mov%cwq %d(%s), %%rax\n", sign ? 's' : 'z', off, from);
		break;
	case 4:
		printf("	%s %d(%s), %s\n", sign ? "movslq" : "movl",
		       off, from, sign ? "%rax" : "%eax");
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

static void someq(struct lval l, const char *inst, const char *outreg)
{
	printf("	push %%rax\n");
	load(l);
	printf("	pop %%rcx\n");
	printf("	%s %%rcx,%%rax\n", inst);
	if (strcmp(outreg, "rax") != 0) printf("	mov %%%s,%%rax\n", outreg);
	store(l);
}

void addeq(struct lval l)
{
	someq(l, "add", "rax");
}

void subeq(struct lval l)
{
	someq(l, "sub", "rax");
}

void muleq(struct lval l)
{
	someq(l, "imul", "rax");
}

void diveq(struct lval l)
{
	someq(l, "idiv", "rax");
}

void remeq(struct lval l)
{
	someq(l, "idiv", "rdx");
}

void bandeq(struct lval l)
{
	someq(l, "and", "rax");
}

void boreq(struct lval l)
{
	someq(l, "or", "rax");
}

void bxoreq(struct lval l)
{
	someq(l, "xor", "rax");
}

void bshleq(struct lval l)
{
	printf("	push %%rax\n");
	load(l);
	printf("	pop %%rcx\n");
	printf("	shl %%cl, %%rax\n");
	store(l);
}

void bshreq(struct lval l)
{
	printf("	push %%rax\n");
	load(l);
	printf("	pop %%rcx\n");
	printf("	shr %%cl, %%rax\n");
	store(l);
}

void idx(struct lval l)
{
	skipws();
	if (*curs != ']') error("expected ']'");
	advcurs(1);

	printf("	add %%rcx, %%rax\n");
	lval.lval_kind = REGIS;

	/* a[i] yields the element behind the pointer */
	if (lval.lval_ty->sty_kind == TYPTR)
		lval.lval_ty = lval.lval_ty->sty_base;

	/*
	 * an array element still decays too: m[i] is an inner array,
	 * and using it re-indexes through a pointer to its first element.
	 */
	if (lval.lval_ty->sty_kind == TYARR)
		lval.lval_ty = mkptr(lval.lval_ty->sty_base);
}

void preinc(struct lval l)
{
	int sz = ptrstep(l.lval_ty);

	printf("	add $%d, %%rax\n", sz);
	store(l);
}

void predec(struct lval l)
{
	int sz = ptrstep(l.lval_ty);

	printf("	sub $%d, %%rax\n", sz);
	store(l);
}

void inc(struct lval l)
{
	int sz = ptrstep(l.lval_ty);

	printf("	add $%d, %%rax\n", sz);
	store(l);
}

void dec(struct lval l)
{
	int sz = ptrstep(l.lval_ty);

	printf("	sub $%d, %%rax\n", sz);
	store(l);
}

void regispre(void)
{
	printf("	push %%rax\n");
}

void regispost(void)
{
	printf("	pop %%rcx\n");
}

/*
 * called at the end of expr() when the result sits behind the pointer
 * in %%rax. at that point lval_ty already is the pointee type.
 */
void deptr(struct lval l)
{
	if (l.lval_ty->sty_size == 0)
		error("cannot dereference a void pointer");
	load_mem(0, "%rax", l.lval_ty->sty_size, l.lval_ty->sty_signed);
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

void beginframe(int setup, int body)
{
	/*
	 * the stack frame can only be sized after every declaration has
	 * been parsed, so the prologue runs from the end of the function:
	 * jump to a setup block, leave a body label behind, and let the
	 * setup (emitted by endframe) jump back once the frame is known.
	 */
	printf("	jmp .L%d\n", setup);
	printf(".L%d:\n", body);
}

void endframe(int setup, int body, int size)
{
	printf(".L%d:\n", setup);
	printf("	push %%rbp\n");
	printf("	mov %%rsp, %%rbp\n");
	printf("	sub $%d, %%rsp\n", size);
	printf("	jmp .L%d\n", body);
}

void epilogue(void)
{
	printf("	mov %%rbp, %%rsp\n");
	printf("	pop %%rbp\n");
}

void load(struct lval l)
{
	int size = stysize(l.lval_ty);
	int sign = l.lval_ty->sty_signed;

	if (l.lval_kind == REGIS) {
		load_mem(0, "%rax", size, sign);
		return;
	}

	load_mem(l.lval_off, "%rbp", size, sign);
}

void store(struct lval l)
{
	int size = stysize(l.lval_ty);

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

void cast(struct symty *ty)
{
	/* only the width matters: rax already holds the whole value */
	if (ty->sty_kind != TYSCALR) return;

	switch (ty->sty_size) {
	case 1:
		printf("	mov%cb %%al, %%eax\n", ty->sty_signed ? 's' : 'z');
		break;
	case 2:
		printf("	mov%cw %%ax, %%eax\n", ty->sty_signed ? 's' : 'z');
		break;
	case 4:
		printf("	%s %%eax, %s\n", ty->sty_signed ? "movslq" : "movl",
		       ty->sty_signed ? "%rax" : "%eax");
		break;
	default:
		break;
	}
}

void retval(unsigned long long val)
{
	printf("	mov $%llu,%%rax\n", val);
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

void and(struct lval)
{
	printf("	test %%rax,%%rax\n");
	printf("	setne %%al\n");
	printf("	movzbq %%al,%%rax\n");
	printf("	test %%rcx,%%rcx\n");
	printf("	setne %%cl\n");
	printf("	movzbq %%cl,%%rcx\n");
	printf("	and %%rcx,%%rax\n");
}

void or(struct lval)
{
	printf("	test %%rax,%%rax\n");
	printf("	setne %%al\n");
	printf("	movzbq %%al,%%rax\n");
	printf("	test %%rcx,%%rcx\n");
	printf("	setne %%cl\n");
	printf("	movzbq %%cl,%%rcx\n");
	printf("	or %%rcx,%%rax\n");
}

void band(struct lval)
{
	printf("	and %%rcx, %%rax\n");
}

void bor(struct lval)
{
	printf("	or %%rcx, %%rax\n");
}

void bxor(struct lval)
{
	printf("	xor %%rcx, %%rax\n");
}

void bshl(struct lval)
{
	printf("	shl %%cl, %%rax\n");
}

void bshr(struct lval)
{
	printf("	shr %%cl, %%rax\n");
}

#include <stdio.h>
#include <string.h>

#include "emit.h"
#include "parse.h"
#include "util.h"

/*
 * load/store templates, indexed [sign][size] and [size]: sizes are 1, 2, 4
 * and 8 bytes so they land in the middle of the table, holes stay NULL.
 */
static const char *const ldtpl[2][9] = {
	{ NULL, "\tmovzbq %d(%s), %%rax\n", "\tmovzwq %d(%s), %%rax\n", NULL,
	  "\tmovl %d(%s), %%eax\n", NULL, NULL, NULL, "\tmov %d(%s), %%rax\n" },
	{ NULL, "\tmovsbq %d(%s), %%rax\n", "\tmovswq %d(%s), %%rax\n", NULL,
	  "\tmovslq %d(%s), %%rax\n", NULL, NULL, NULL, "\tmov %d(%s), %%rax\n" },
};

static void load_mem(int off, const char *from, int size, int sign)
{
	if (!ldtpl[sign][size]) error("not yet implemented - load");
	printf(ldtpl[sign][size], off, from);
}

static const char *const sttpl[9] = {
	NULL, "\tmovb %%al, %d(%s)\n", "\tmovw %%ax, %d(%s)\n", NULL,
	"\tmovl %%eax, %d(%s)\n", NULL, NULL, NULL, "\tmovq %%rax, %d(%s)\n",
};

static void store_mem(int off, const char *to, int size)
{
	if (!sttpl[size]) error("not implemented yet - store");
	printf(sttpl[size], off, to);
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
	(void)l;
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

/*
 * the fixed-string templates from the operator/unary tables go out raw;
 * anything needing a live lvalue uses its emitter function instead.
 */
void emit(const char *s)
{
	fputs(s, stdout);
}

void runemit(void (*fn)(struct lval), const char *tpl, struct lval l)
{
	if (fn) fn(l); else emit(tpl);
}

/* turn a pending compile-time value into a real rax result */
void materialize(struct lval *lv)
{
	if (!lv->lval_isconst) return;
	retval(lv->lval_val);
	lv->lval_isconst = 0;
}

void cast(struct symty *ty)
{
	static const char *const casttpl[2][9] = {
		{ NULL, "movzbl %%al, %%eax", "movzwl %%ax, %%eax", NULL,
		  "movl %%eax, %%eax", NULL, NULL, NULL, NULL },
		{ NULL, "movsbl %%al, %%eax", "movswl %%ax, %%eax", NULL,
		  "movslq %%eax, %%rax", NULL, NULL, NULL, NULL },
	};

	/* only the width matters: rax already holds the whole value */
	if (ty->sty_kind != TYSCALR || !casttpl[ty->sty_signed][ty->sty_size])
		return;
	printf("	%s\n", casttpl[ty->sty_signed][ty->sty_size]);
}

void retval(unsigned long long val)
{
	printf("	mov $%llu,%%rax\n", val);
}

void ret(void)
{
	printf("	ret\n");
}

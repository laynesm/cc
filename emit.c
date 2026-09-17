#include <stdio.h>
#include <string.h>

#include "emit.h"
#include "parse.h"
#include "util.h"

/*
 * load/store templates, indexed [sign][size] and [size]: sizes are 1, 2, 4
 * and 8 bytes so they land in the middle of the table, holes stay NULL.
 * the address comes in whole, since a file-scope object's operand is
 * 'g(%rip)', not a bare displacement.
 */
static const char *const ldtpl[2][9] = {
	{NULL, "\tmovzbq %s, %%rax\n", "\tmovzwq %s, %%rax\n", NULL, "\tmovl %s, %%eax\n", NULL, NULL, NULL,
         "\tmov %s, %%rax\n"},
	{NULL, "\tmovsbq %s, %%rax\n", "\tmovswq %s, %%rax\n", NULL, "\tmovslq %s, %%rax\n", NULL, NULL, NULL,
         "\tmov %s, %%rax\n"},
};

static void load_mem(const char *op, int size, int sign)
{
	if (!ldtpl[sign][size]) error("not yet implemented - load");
	printf(ldtpl[sign][size], op);
}

static const char *const sttpl[9] = {
	NULL, "\tmovb %%al, %s\n",  "\tmovw %%ax, %s\n", NULL, "\tmovl %%eax, %s\n", NULL, NULL,
	NULL, "\tmovq %%rax, %s\n",
};

static void store_mem(const char *op, int size)
{
	if (!sttpl[size]) error("not implemented yet - store");
	printf(sttpl[size], op);
}

/*
 * the memory operand behind an l-value: a frame displacement or a
 * %rip-relative file-scope object.
 */
static const char *memop(struct lval l, char *buf, size_t len)
{
	if (l.lval_isglob)
		snprintf(buf, len, "%s(%%rip)", l.lval_glob);
	else
		snprintf(buf, len, "%d(%%rbp)", l.lval_off);
	return buf;
}

enum {
	QADD, QSUB, QMUL, QDIV, QREM, QBAND, QBOR, QBXOR, QNUM
};

struct qop {
	const char *inst;
	int         iscqo;
	const char *outreg;
};

/* the compound binary assignments differ only in the opcode */
static const struct qop qops[QNUM] = {
	[QADD]  = {"add", 0, "rax"},
	[QSUB]  = {"sub", 0, "rax"},
	[QMUL]  = {"imul", 0, "rax"},
	[QDIV]  = {"idiv", 1, "rax"},
	[QREM]  = {"idiv", 1, "rdx"},
	[QBAND] = {"and", 0, "rax"},
	[QBOR]  = {"or", 0, "rax"},
	[QBXOR] = {"xor", 0, "rax"},
};

static void someq(struct lval l, int idx)
{
	/* x = x op v: park the lhs, reload the lvalue, unpark and reuse */
	park();
	load(l);
	unpark();

	if (qops[idx].iscqo) {
		/* idiv wants the dividend sign-extended into rdx */
		printf("	cqo\n");
		printf("	idiv %%rcx\n");
	} else {
		printf("	%s %%rcx,%%rax\n", qops[idx].inst);
	}

	if (strcmp(qops[idx].outreg, "rax") != 0) printf("	mov %%%s,%%rax\n", qops[idx].outreg);
	store(l);
}

void addeq(struct lval l) { someq(l, QADD); }
void subeq(struct lval l) { someq(l, QSUB); }
void muleq(struct lval l) { someq(l, QMUL); }
void diveq(struct lval l) { someq(l, QDIV); }
void remeq(struct lval l) { someq(l, QREM); }
void bandeq(struct lval l) { someq(l, QBAND); }
void boreq(struct lval l) { someq(l, QBOR); }
void bxoreq(struct lval l) { someq(l, QBXOR); }

void bshleq(struct lval l)
{
	park();
	load(l);
	unpark();
	printf("	shl %%cl, %%rax\n");
	store(l);
}

void bshreq(struct lval l)
{
	park();
	load(l);
	unpark();
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
	if (lval.lval_ty->sty_kind == TYPTR) lval.lval_ty = lval.lval_ty->sty_base;

	/*
	 * an array element still decays too: m[i] is an inner array,
	 * and using it re-indexes through a pointer to its first element.
	 */
	if (lval.lval_ty->sty_kind == TYARR) lval.lval_ty = mkptr(lval.lval_ty->sty_base);
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

void park(void)
{
	stkpend += 8;
	printf("	push %%rax\n");
}

void unpark(void)
{
	stkpend -= 8;
	printf("	pop %%rcx\n");
}

/*
 * called at the end of expr() when the result sits behind the pointer
 * in %%rax. at that point lval_ty already is the pointee type.
 */
void deptr(struct lval l)
{
	if (l.lval_ty->sty_kind == TYFUNC) return; /* a function designator *is* its address */
	if (l.lval_ty->sty_size == 0) error("cannot dereference a void pointer");
	load_mem("0(%rax)", l.lval_ty->sty_size, l.lval_ty->sty_signed);
}

void ptr(struct lval l)
{
	char buf[SYMMAX + 8];

	printf("	lea %s, %%rax\n", memop(l, buf, sizeof(buf)));
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

	/*
	 * the frame is rounded up to a 16-byte multiple so that, from the
	 * prologue on, rsp stays 16-aligned as long as the expression
	 * machinery keeps its pushes balanced. a call site then only needs
	 * to account for its own argument stack in the alignment padding.
	 */
	size = (size + 15) & ~15;
	printf("	sub $%d, %%rsp\n", size);

	fparamprologue();
	printf("	jmp .L%d\n", body);
}

void epilogue(void)
{
	printf("	mov %%rbp, %%rsp\n");
	printf("	pop %%rbp\n");
}

void load(struct lval l)
{
	char buf[SYMMAX + 8];
	int  size = stysize(l.lval_ty);
	int  sign = l.lval_ty->sty_signed;

	if (l.lval_kind == REGIS) {
		load_mem("0(%rax)", size, sign);
		return;
	}

	load_mem(memop(l, buf, sizeof(buf)), size, sign);
}

void store(struct lval l)
{
	char buf[SYMMAX + 8];
	int  size = stysize(l.lval_ty);

	if (l.lval_kind == REGIS) {
		store_mem("0(%rcx)", size);
		return;
	}

	store_mem(memop(l, buf, sizeof(buf)), size);
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
	if (fn)
		fn(l);
	else
		emit(tpl);
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
	/* printed through %s like every template, so registers carry one '%' */
	static const char *const casttpl[2][9] = {
		{NULL, "movzbl %al, %eax", "movzwl %ax, %eax", NULL, "movl %eax, %eax", NULL, NULL, NULL, NULL},
		{NULL, "movsbl %al, %eax", "movswl %ax, %eax", NULL, "movslq %eax, %rax", NULL, NULL, NULL, NULL},
	};

	/* only the width matters: rax already holds the whole value */
	if (ty->sty_kind != TYSCALR || !casttpl[ty->sty_signed][ty->sty_size]) return;
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

/*
 * the assembler starts in .text; a switch only happens when a global
 * object forces the output into .data, and the next function body or
 * .comm directive must not inherit it.
 */
static int intext = 1;

void sectext(void)
{
	if (intext) return;
	printf("	.text\n");
	intext = 1;
}

void sectdata(void)
{
	if (!intext) return;
	printf("	.data\n");
	intext = 0;
}

void globcomm(const char *name, int size, int align)
{
	printf("	.comm %s,%d,%d\n", name, size, align);
}

/* sizes are powers of two: 1/2/4/8 map to .byte/.word/.long/.quad */
static const char *const sztpl[9] = {
	NULL, ".byte ", ".word ", NULL, ".long ", NULL, NULL, NULL, ".quad ",
};

void globdata(const char *name, int size, int align, unsigned long long val)
{
	sectdata();
	printf("	.p2align %d\n", sizlog(align));
	printf("	.globl %s\n", name);
	lbl(name);

	if (size <= 0 || size > 8 || !sztpl[size]) error("cannot lay out global '%s'", name);
	printf("	%s%llu\n", sztpl[size], val);
}

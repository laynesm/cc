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

	/*
	 * a value the parser knows at compile time. when the flag is set no
	 * asm has been emitted for it yet; materialize() turns it into rax.
	 */
	int           lval_isconst;
	unsigned long long lval_val;

	/*
	 * when set the l-value lives in a file-scope object: its address is
	 * lval_glob(%rip), not an %rbp displacement.
	 */
	int           lval_isglob;
	char          lval_glob[SYMMAX];
};

extern struct lval lval;

void materialize(struct lval *);

/*
 * assembly templates. only printed through emit() (fputs), so the strings
 * carry bare register names: '%rax', not printf's '%%rax'.
 */
#define EMITNOP  ""
#define EMITADD  "\tadd %rcx, %rax\n"
#define EMITSUB  "\tsub %rcx, %rax\n"
#define EMITMUL  "\timul %rcx, %rax\n"
#define EMITIDIV "\tcqo\n\tidiv %rcx\n"
#define EMITREM  "\tcqo\n\tidiv %rcx\n\tmov %rdx, %rax\n"
#define EMITBAND "\tand %rcx, %rax\n"
#define EMITBOR  "\tor %rcx, %rax\n"
#define EMITBXOR "\txor %rcx, %rax\n"
#define EMITBSHL "\tshl %cl, %rax\n"
#define EMITBSHR "\tshr %cl, %rax\n"
#define EMITNEG  "\tneg %rax\n"
#define EMITBNOT "\tnot %rax\n"

/* the six comparisons differ by their setcc suffix alone */
#define EMITSET(cc) \
	"\tcmp %rcx, %rax\n\tset" cc " %al\n\tmovzbq %al, %rax\n"
#define EMITEQ  EMITSET("e")
#define EMITNE  EMITSET("ne")
#define EMITLT  EMITSET("l")
#define EMITGT  EMITSET("g")
#define EMITLE  EMITSET("le")
#define EMITGE  EMITSET("ge")

#define EMITLNOT "\tcmp $0, %rax\n\tsete %al\n\tmovzbq %al, %rax\n"

/* the boolean ops share the not-materialization, differ by the last step */
#define EMITLOGIC(op)                                                        \
	"\ttest %rax,%rax\n\tsetne %al\n\tmovzbq %al,%rax\n"                 \
	"\ttest %rcx,%rcx\n\tsetne %cl\n\tmovzbq %cl,%rcx\n"                 \
	"\t" op " %rcx,%rax\n"
#define EMITAND EMITLOGIC("and")
#define EMITOR  EMITLOGIC("or")

void emit(const char *);
void runemit(void (*)(struct lval), const char *, struct lval);

/*
 * the balanced push/pop around an rhs or an l-value: park() stashes the
 * value in %rax on the stack and credits stkpend, unpark() fetches it
 * back into %rcx and settles the counter again.
 */
void park(void);
void unpark(void);

void deptr(struct lval);
void ptr(struct lval);

void inc(struct lval);
void dec(struct lval);

void bandeq(struct lval);
void boreq(struct lval);
void bxoreq(struct lval);
void bshleq(struct lval);
void bshreq(struct lval);

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
void beginframe(int, int);
void endframe(int, int, int);
void epilogue(void);
void load(struct lval);
void store(struct lval);
void lbl(const char *);
void globl(const char *);
void retval(unsigned long long);
void ret(void);
void cast(struct symty *);

/* a file-scope object: .data with a constant value, or .comm (tentative) */
void globdata(const char *, int, int, unsigned long long);
void globcomm(const char *, int, int);

/* switch the assembly section, only when it actually changes */
void sectext(void);
void sectdata(void);

#endif

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cc.h"
#include "emit.h"
#include "ident.h"
#include "keywords.h"
#include "parse.h"
#include "type.h"
#include "util.h"

struct lval lval;

struct symty *expr_ty;

/*
 * subscript into optbl of the operator that glued the whole expression
 * together. only the outermost expr() call fills it, so a condition
 * knows whether its root was a bare assignment ('if (x=1)').
 */
int expr_rootop;

/* a dereference deferred because a postfix operator binds first */
static int derefpend;

/* set by 'extern', cleared by the declaration that consumes it */
int extdecl;

/* the statement mode currently being parsed (STMT vs DECEXP) */
static int curstmt;

/*
 * whether the statement being parsed has emitted a side effect
 * (an assignment or ++/--). read at the end of the expression
 * statement: false means the statement does nothing.
 */
static int effect;

/*
 * basic assignment type checking.
 * pointers are strict, scalars only warn.
 */
static void tyassign(struct symty *dst, struct symty *src)
{
	if (dst->sty_kind == TYPTR || src->sty_kind == TYPTR) {
		if (src->sty_kind != TYPTR)
			error("assignment makes pointer from integer without a "
			      "cast");
		if (dst->sty_kind != TYPTR)
			error("assignment makes integer from pointer without a "
			      "cast");

		/*
		 * a void* is the generic pointer: it freely converts to and
		 * from any other pointer type.
		 */
		if (dst->sty_base->sty_size == 0 || src->sty_base->sty_size == 0) return;

		if (!tyeq(dst->sty_base, src->sty_base)) error("assignment from incompatible pointer type");
		return;
	}

	if (dst->sty_signed != src->sty_signed && src->sty_size >= dst->sty_size)
		warn("implicit conversion changes signedness");

	if (src->sty_size > dst->sty_size)
		warn("implicit conversion loses %d bits of precision", (src->sty_size - dst->sty_size) * 8);
}

/*
 * a type keyword that turned into a function. the signature is recorded
 * in the funtab; a body following the declarator gets its own frame and
 * prologue, a plain ';' keeps it a prototype. the shared stack-offset
 * counter is parked back to its pre-function base either way, so the
 * next declaration starts a fresh accounting. returns whether a body was
 * parsed: a definition ends the declaration, it carries no trailing ';'.
 */
static int fndecl(const char *name, struct symty *ty, int framesave)
{
	int setup = newlbl();
	int body  = newlbl();

	if (*curs != '{') {
		/* a prototype: no code, the parameters never become frame slots */
		funcadd(name, ty, 0);
		symdrop(depth + 1);
		symrestore(framesave);
		return 0;
	}

	funcadd(name, ty, 1);
	funcretty  = ty->sty_base;
	funcretlbl = newlbl();
	labreset();

	sectext();
	globl(name);
	beginframe(setup, body);

	stmt(STMT); /* the body block */

	labcheck();
	idlbl(funcretlbl);
	epilogue();
	ret();
	endframe(setup, body, framesize());

	symrestore(framesave);

	/* a stray ';' after the '}' is a null statement, not part of this one */
	skipws();
	if (*curs == ';') advcurs(1);
	return 1;
}

void decl(struct symty *ty)
{
	int remaining = 0;
	int framesave = symsave();
	int cls       = extdecl ? SCEXT : (curstmt == STMT && depth == 0) ? SCGLOB : SCLOCAL;

	extdecl = 0;

	do {
		struct sym   *s;
		struct symty *curty;

		skipws();
		declname[0] = '\0';
		curty       = declarator(ty);

		if (declname[0] == '\0') error("declaration without a name");

		if (curty->sty_kind == TYFUNC) {
			/* a body already consumed its own ';' */
			if (fndecl(declname, curty, framesave)) break;
			skipws();
		} else {
			s = symadd(declname, depth, curty, cls);

			/*
			 * a zero-sized type is the void type: it may only show up
			 * behind a pointer ('void *') or as a function result,
			 * never as a standalone object.
			 */
			if (stysize(curty) == 0) error("variable '%s' cannot be void", s->sym_name);

			skipws();

			if (cls == SCLOCAL) {
				if (*curs == '=') {
					if (curty->sty_kind == TYARR) error("array initializer not yet supported");
					advcurs(1);
					expr_ty = s->sym_ty;
					expr(-1);
					tyassign(s->sym_ty, lval.lval_ty);
				} else if (curty->sty_kind == TYARR) {
					/*
					 * a default array has no single element to store:
					 * zero out the whole reserved region in one go.
					 */
					printf("\tlea %d(%%rbp), %%rdi\n", s->sym_off);
					printf("\txor %%eax, %%eax\n");
					printf("\tmov $%d, %%rcx\n", stysize(curty));
					printf("\trep stosb\n");
				} else {
					printf("\txor %%rax,%%rax\n");
				}

				/*
				 * an array has no single value to store: it only reserves
				 * its stack space.
				 */
				if (curty->sty_kind != TYARR) {
					materialize(&lval);
					lval.lval_off  = s->sym_off;
					lval.lval_kind = STACK;
					lval.lval_ty   = s->sym_ty;
					store(lval);
				}
			} else {
				/*
				 * a file-scope object accepts only a constant
				 * initializer: no code may run before the program does.
				 */
				if (*curs == '=') {
					advcurs(1);
					expr_ty = s->sym_ty;
					expr(-1);
					tyassign(s->sym_ty, lval.lval_ty);

					if (!lval.lval_isconst) error("initializer element is not constant");

					if (cls == SCGLOB) {
						if (s->sym_done) error("redefinition of '%s'", s->sym_name);
						globdata(s->sym_name, stysize(curty), symalign(curty), lval.lval_val);
						s->sym_done = 1;
					}
				} else if (cls == SCGLOB && !s->sym_done) {
					/* a tentative object: common storage, once */
					globcomm(s->sym_name, stysize(curty), symalign(curty));
				}
			}
		}

		remaining = 0;

		if (*curs == ',') {
			advcurs(1);
			remaining = 1;
		} else if (*curs != ';') {
			error("unexpected character");
		}
	} while (remaining);
}

void factor(void)
{
	const struct unary *un;
	unsigned long long  val;
	char               *end;
	int                 base;

	skipws();
	un = unopundercurs();
	if (un) {
		advcurs(un->un_slen);
		factor();

		if (un->un_assoc == OPASSOCR) {
			struct lval l = lval;

			/*
			 * only ++/-- (PTRARITH) write: unary '*' and '&' just
			 * shape up a value, so they never count as an effect.
			 */
			if (un->un_ptr == PTRARITH) effect = 1;

			if (un->un_ptr == DEPTR) {
				if (l.lval_ty->sty_kind != TYPTR) error("cannot dereference non-pointer");

				/*
				 * '[' binds tighter than '*': '*ap[1]' is
				 * '*(ap[1])'. if a postfix operator sits right
				 * behind the operand, resolve it first and
				 * dereference its result instead.
				 */
				if (opundercurs() && opundercurs()->op_postfix) {
					derefpend = 1;
					return;
				}

				/*
				 * a register l-value still holds the address of
				 * its own object: read the object so the pending
				 * dereference has a pointer to work on.
				 */
				if (l.lval_kind == REGIS) {
					load(l);
					l.lval_kind = NONE;
				}

				l.lval_ty = l.lval_ty->sty_base;
			} else if (l.lval_off == NONE && !l.lval_isglob) {
				error("l-value required for unary '%s' operand", un->un_str);
			}

			if (un->un_ptr == GENPTR) l.lval_ty = mkptr(l.lval_ty);

			lval.lval_kind    = un->un_genlval;
			lval.lval_off     = NONE;
			lval.lval_ty      = l.lval_ty;
			lval.lval_isconst = 0;
			lval.lval_isglob  = 0;

			runemit(un->un_emit, un->un_tpl, l);
			return;
		}

		/* the algebraic unaries fold a compile-time operand */
		if (!lval.lval_isconst) {
			runemit(un->un_emit, un->un_tpl, lval);
			return;
		}

		switch (*un->un_str) {
		case '-':
			lval.lval_val = (unsigned long long)-(long long)lval.lval_val;
			break;
		case '~':
			lval.lval_val = ~lval.lval_val;
			break;
		case '!':
			lval.lval_val = lval.lval_val == 0;
			break;
		default:
			break; /* unary '+' */
		}
		return;
	}

	if (*curs == '(') {
		const struct keyword *kw;
		struct symty         *cty;
		char                 *savcurs = curs;

		advcurs(1);
		kw = peekword();

		/*
		 * a type keyword right after '(' is the leading edge of a
		 * cast. parsety() (shared with doty) decides the type.
		 */
		if (kw && kw->kw_func == doty) {
			struct lval l;
			char        buf[KWMAX];

			skipws();
			kw  = readword(buf, sizeof(buf));
			cty = parsety(kw->kw_id);
			skipws();
			cty = declarator(cty);
			skipws();
			if (*curs != ')') error("expected ')' after cast");
			advcurs(1);

			factor();
			l = lval;
			if (l.lval_kind == REGIS) deptr(l);
			materialize(&lval);
			cast(cty);
			lval.lval_kind    = NONE;
			lval.lval_off     = NONE;
			lval.lval_ty      = cty;
			lval.lval_isconst = 0;
			lval.lval_isglob  = 0;
			return;
		}

		curs = savcurs;
		advcurs(1);
		expr(-1);
		skipws();
		if (*curs != ')') error("expected )");
		advcurs(1);
		return;
	}

	if (isalpha(*curs) || *curs == '_' || *curs == '$') {
		char       *savcurs = curs;
		struct sym *s;
		char        name[SYMMAX];

		readident(name, sizeof(name));
		skipws();

		s = symlookup(name, -1);
		if (!s) {
			struct func *f = funclookup(name);

			if (!f) {
				curs = savcurs;
				error("undefined usage of '%s'", name);
			}

			/* a function name is the address of its entry point */
			printf("	lea %s(%%rip), %%rax\n", f->func_name);
			lval.lval_kind    = NONE;
			lval.lval_off     = NONE;
			lval.lval_ty      = mkptr(f->func_ty);
			lval.lval_isconst = 0;
			lval.lval_isglob  = 0;
			return;
		}

		/*
		 * a file-scope object is addressed by name, not by a frame
		 * slot. externs address the same way: the linker fills in.
		 */
		if (s->sym_stcls >= SCGLOB) {
			if (s->sym_ty->sty_kind == TYARR) {
				printf("	lea %s(%%rip), %%rax\n", s->sym_name);
				lval.lval_kind    = NONE;
				lval.lval_off     = s->sym_off;
				lval.lval_ty      = mkptr(s->sym_ty->sty_base);
				lval.lval_isconst = 0;
				lval.lval_isglob  = 0;
				return;
			}

			lval.lval_kind   = STACK;
			lval.lval_off    = NONE;
			lval.lval_isglob = 1;
			strncpy(lval.lval_glob, s->sym_name, sizeof(lval.lval_glob) - 1);
			lval.lval_glob[sizeof(lval.lval_glob) - 1] = '\0';
			lval.lval_ty                               = s->sym_ty;
			lval.lval_isconst                          = 0;
			load(lval);
			return;
		}

		/*
		 * outside an initializer an array has no value: using its
		 * name decays it to a pointer to its first element.
		 */
		if (s->sym_ty->sty_kind == TYARR) {
			printf("	lea %d(%%rbp), %%rax\n", s->sym_off);
			lval.lval_kind    = NONE;
			lval.lval_off     = s->sym_off;
			lval.lval_ty      = mkptr(s->sym_ty->sty_base);
			lval.lval_isconst = 0;
			lval.lval_isglob  = 0;
			return;
		}

		lval.lval_kind    = STACK;
		lval.lval_off     = s->sym_off;
		lval.lval_ty      = s->sym_ty;
		lval.lval_isconst = 0;
		lval.lval_isglob  = 0;
		load(lval);
		return;
	}

	errno = 0;
	if (*curs == '0') {
		char c = tolower(curs[1]);

		base = c == 'x' ? 16 : c == 'd' ? 10 : c == 'b' ? 2 : c == 'o' ? 8 : 8;

		if (c == 'x' || c == 'd' || c == 'b' || c == 'o') {
			if (!isdigit(curs[2])) error("incomplete literal");
			curs += 2;
		}
	} else {
		base = 10;
	}

	val = strtoull(curs, &end, base);
	if (errno == ERANGE) error("integer literal exceeds %llu", ULLONG_MAX);
	if (end == curs) error("expected expression");

	curs = end;
	skipws();

	lval.lval_kind    = NONE;
	lval.lval_ty      = expr_ty->sty_kind == TYPTR ? defty : inferty(val);
	lval.lval_isconst = 1;
	lval.lval_isglob  = 0;
	lval.lval_val     = val;
}

void pointarith(const struct operator *op, struct symty *lhsty, struct symty *rhsty, int iscompound)
{
	int islhsptr = lhsty->sty_kind == TYPTR;
	int isrhsptr = rhsty->sty_kind == TYPTR;

	/* the two pointer flags add up to a single case number */
	if (islhsptr + isrhsptr == 2) error("both operands are pointers");
	if (islhsptr + isrhsptr == 0) return;
	if (iscompound && isrhsptr && !islhsptr) error("assignment makes integer from pointer without a cast");
	if (op->op_ptr == NOPTR) error("invalid pointer operation");

	struct symty *ptrty = islhsptr ? lhsty : rhsty;
	const char   *reg   = islhsptr && !iscompound ? "%rcx" : "%rax";
	int           sz    = ptrstep(ptrty);

	if (isrhsptr) *lhsty = *rhsty;
	if (sz > 1) printf("	imul $%d, %s\n", sz, reg);
}

/*
 * completes a deferred dereference: '*ap[1]' waits for the postfix
 * chain, reads the pointer stored in the indexed object and switches
 * to the pointed-to type.
 */
static void derefvalue(struct lval *lv)
{
	if (!derefpend) return;

	if (lv->lval_ty->sty_kind != TYPTR) error("cannot dereference non-pointer");
	load(*lv);
	lv->lval_ty = lv->lval_ty->sty_base;
	derefpend   = 0;
}

/*
 * compile-time evaluation: mirrors what the emitted asm would do, so the
 * fold and the machine agree. the operator's index is its table position.
 * returns 0 when the operator has no fold or the fold must not happen.
 */
static int foldop(const struct operator *op, unsigned long long l, unsigned long long r, unsigned long long *out)
{
	switch (op - optbl) {
	case OPADD:
		*out = l + r;
		break;
	case OPSUB:
		*out = l - r;
		break;
	case OPMUL:
		*out = l * r;
		break;
	case OPSHL:
		*out = l << (r & 63);
		break;
	case OPSHR:
		*out = l >> (r & 63);
		break;
	case OPBAND:
		*out = l & r;
		break;
	case OPBOR:
		*out = l | r;
		break;
	case OPXOR:
		*out = l ^ r;
		break;

	/* idiv is signed; a zero divisor keeps the runtime crash */
	case OPDIV:
		if (r == 0) return 0;
		*out = (unsigned long long)((long long)l / (long long)r);
		break;
	case OPREM:
		if (r == 0) return 0;
		*out = (unsigned long long)((long long)l % (long long)r);
		break;

	/* compares and the boolean ops narrow to 0/1 */
	case OPEQ:
		*out = l == r;
		break;
	case OPNEQ:
		*out = l != r;
		break;
	case OPLT:
		*out = (long long)l < (long long)r;
		break;
	case OPGT:
		*out = (long long)l > (long long)r;
		break;
	case OPLE:
		*out = (long long)l <= (long long)r;
		break;
	case OPGE:
		*out = (long long)l >= (long long)r;
		break;
	case OPAND:
		*out = l != 0 && r != 0;
		break;
	case OPOR:
		*out = l != 0 || r != 0;
		break;

	default:
		return 0;
	}
	return 1;
}

void expr(int min_prec)
{
	struct symty *saved   = expr_ty;
	int           savedep = derefpend;
	static int    depth;

	/*
	 * the root operator of the whole expression is the one consumed at
	 * the outermost call: everything nested works on a sub-expression.
	 * every top-level call starts a fresh expression, so the output
	 * slot needs no save/restore across them.
	 */
	if (depth == 0) expr_rootop = -1;
	depth++;

	derefpend = 0;

	factor();

	for (;;) {
		const struct operator *op = opundercurs();
		struct symty          *lhs_ty;

		if (!op || op->op_precedence < min_prec) break;
		advcurs(op->op_slen);

		if (depth == 1) expr_rootop = (int)(op - optbl);

		if (op->op_assoc == OPASSOCR) {
			struct lval l = lval;
			if (l.lval_kind == NONE) error("assignment without an l-value.");
			derefvalue(&l);

			/* an assignment always writes, whatever its outcome */
			effect = 1;

			if (l.lval_kind == REGIS) regispre();

			if (op->op_assign != ASMOD) {
				expr_ty = l.lval_ty;
				expr(op->op_precedence);
			}

			if (l.lval_kind == REGIS) regispost();

			materialize(&lval);

			if (op->op_assign != ASMOD) {
				if (op->op_assign == ASTORE ||
				    (l.lval_ty->sty_kind != TYPTR && lval.lval_ty->sty_kind != TYPTR))
					tyassign(l.lval_ty, lval.lval_ty);

				if (op->op_assign != ASTORE) pointarith(op, l.lval_ty, lval.lval_ty, 1);
			}

			runemit(op->op_emit, op->op_tpl, l);

			lval.lval_kind    = NONE;
			lval.lval_ty      = l.lval_ty;
			lval.lval_isconst = 0;
			lval.lval_isglob  = 0;
			continue;
		}

		/*
		 * a register l-value is a pending dereference: the postfix
		 * '[' keeps the address, anything that follows needs the value.
		 */
		if (lval.lval_kind == REGIS && !op->op_postfix) {
			derefvalue(&lval);
			deptr(lval);
			lval.lval_kind    = NONE;
			lval.lval_isconst = 0;
			lval.lval_isglob  = 0;
		}

		/*
		 * If it is left-associated, overwrite the l-value.
		 */
		{
			int                lhs_cst = lval.lval_isconst;
			unsigned long long lhsv    = lval.lval_val;

			lhs_ty         = lval.lval_ty;
			lval.lval_kind = lval.lval_off = NONE;
			lval.lval_isconst              = 0;
			lval.lval_isglob               = 0;

			if (lhs_cst) {
				/* a pending constant costs nothing to keep */
				expr_ty = lhs_ty;
				expr(op->op_postfix ? -1 : op->op_precedence + 1);

				/*
				 * both operands were compile-time values: fold in C
				 * and emit nothing. a pointer lhs is skipped --
				 * pointer values never leave the type system as
				 * consts.
				 */
				if (lval.lval_isconst && lhs_ty->sty_kind != TYPTR &&
				    foldop(op, lhsv, lval.lval_val, &lval.lval_val))
					continue;

				/* park the materialized rhs, reload the lhs */
				materialize(&lval);
				printf("	push %%rax\n");
				printf("	pop %%rcx\n");
				retval(lhsv);
			} else {
				printf("	push %%rax\n");
				expr_ty = lhs_ty;
				expr(op->op_postfix ? -1 : op->op_precedence + 1);

				if (lval.lval_isconst) {
					/* rhs is a compile-time value: an immediate */
					printf("	pop %%rcx\n");
					printf("	mov $%llu, %%rcx\n", lval.lval_val);
					lval.lval_isconst = 0;
				} else {
					printf("	pop %%rcx\n");
					printf("	xchg %%rax, %%rcx\n");
					lval.lval_isglob = 0;
				}
			}

			if (op->op_ptr == PTRARITH || op->op_ptr == NOPTR) pointarith(op, lhs_ty, lval.lval_ty, 0);

			lval.lval_ty = lhs_ty;
			runemit(op->op_emit, op->op_tpl, lval);
		}
	}

	if (lval.lval_kind == REGIS) {
		derefvalue(&lval);
		deptr(lval);
		lval.lval_kind = NONE;
	}

	derefpend = savedep;
	expr_ty   = saved;
	depth--;
}

struct lab labdefs[LABMAX];
int        nlabdefs;
struct lab labuses[LABMAX];
int        nlabuses;

void labreset(void)
{
	nlabdefs = nlabuses = 0;
}

static int labhas(const struct lab labs[], int n, const char *name)
{
	for (int i = 0; i < n; i++)
		if (strcmp(labs[i].lab_name, name) == 0) return 1;
	return 0;
}

/* a 'name:' statement: record it, reject a second definition */
void labadddef(const char *name)
{
	if (nlabdefs == LABMAX) error("too many labels in function");
	if (labhas(labdefs, nlabdefs, name)) error("redefinition of label '%s'", name);
	strncpy(labdefs[nlabdefs].lab_name, name, SYMMAX - 1);
	labdefs[nlabdefs].lab_name[SYMMAX - 1] = '\0';
	nlabdefs++;
}

/* a 'goto name;': remember the target, the definition may come later */
void labadduse(const char *name)
{
	if (labhas(labuses, nlabuses, name)) return;
	if (nlabuses == LABMAX) error("too many labels in function");
	strncpy(labuses[nlabuses].lab_name, name, SYMMAX - 1);
	labuses[nlabuses].lab_name[SYMMAX - 1] = '\0';
	nlabuses++;
}

/* every use must be answered by a definition somewhere in the body */
void labcheck(void)
{
	for (int i = 0; i < nlabuses; i++)
		if (!labhas(labdefs, nlabdefs, labuses[i].lab_name)) error("undefined label '%s'", labuses[i].lab_name);
}

/*
 * 'name := expr;' declares a new block-scope object whose type is taken
 * from the right-hand expression. inherited scalar types are widened to
 * at least int -- a literal '0' would otherwise infer char, and only a
 * character literal (later) may produce a char.
 */
static void inferdecl(char *name)
{
	struct symty *ty;
	struct sym   *s;
	struct lval  lv;

	skipws();
	expr_ty = defty;
	expr(-1);

	ty = lval.lval_ty;
	if (!ty)
		error("cannot infer a type for '%s'", name);
	if (ty->sty_kind == TYSCALR && stysize(ty) < 4)
		ty = sclty(4, 1); /* int */

	skipws();
	if (*curs != ';') error("expected ';' after '%s := ...'", name);
	advcurs(1);

	if (symlookup(name, depth))
		error("redefinition of variable '%s'", name);

	s = symadd(name, depth, ty, SCLOCAL);

	materialize(&lval);
	lv.lval_kind   = STACK;
	lv.lval_off    = s->sym_off;
	lv.lval_isglob = 0;
	lv.lval_ty     = s->sym_ty;
	store(lv);
}

void stmt(int mode)
{
	skipws();
	curstmt = mode;

	if (*curs == ';') {
		/*
		 * smart warn about empty statements
		 * this is not triggered by things like:
		 * while (1);
		 * but it is, in fact, triggered by things that are probable
		 * typos like: 1;;
		 */
		if (line != curs && *(curs - 1) == ';') warn("empty stmt");
		advcurs(1);
		return;
	}

	if (*curs == '{') {
		if (mode != STMT)
			error("a declaration or an expression was expected "
			      "here");
		advcurs(1);
		depth++;

		skipws();
		while (*curs != '}') {
			if (*curs == '\0') error("expected }");
			stmt(STMT);
			skipws();
		}

		advcurs(1);
		symdrop(depth--);
		return;
	}

	if (isalpha(*curs) || curs[0] == '_' || curs[0] == '$') {
		char                  name[1024];
		char                 *savcurs = curs;
		const struct keyword *kw      = readword(name, sizeof(name));

		if (kw) {
			/*
			 * DECEXP only accepts declarations (a type) or plain
			 * expressions, so any other keyword is rejected.
			 */
			if (mode == DECEXP && kw->kw_func != doty)
				error("a declaration or an expression was "
				      "expected "
				      "here");
			/*
			 * this is a feature so we get to parse things like:
			 * if ((int n = stuff()) < 0)
			 * and put the variable 'n' in the correct lifetimee;
			 * a nesting 'depth' level costs nothing, and make it
			 * simpler.
			 */
			depth++;
			kw->kw_func(kw);
			symdrop(depth--);
			return;
		}

		skipws();
		if (*curs == ':' && curs[1] == '=') {
			advcurs(2);
			inferdecl(name);
			return;
		}

		if (*curs == ':') {
			advcurs(1);
			labadddef(name);
			printf(".L_lbl_%s:\n", name);
			stmt(STMT);
			return;
		}

		curs = savcurs;
	}

	effect  = 0;
	expr_ty = defty;
	expr(-1);
	materialize(&lval);
	skipws();
	if (*curs != ';') {
		if (*curs == '\0' || iscntrl(*curs)) error("expected ';'");
		error("trailing characters '%c'", *curs++);
	}

	/*
	 * a statement whose expression performed no write has no observable
	 * effect ('1;', 'x;', 'x + 1;'). assignments and ++/-- set the flag.
	 * DECEXP (the for-init clause) skips the check: 'i;' there is fine.
	 */
	if (mode == STMT && !effect) warn("statement with no effect");

	advcurs(1);
}

/*
 * an old-style definition: the type was already spelled out by a
 * prototype ('int main(int, char **);'), so the definition only lists
 * the name and the parameter identifiers: 'main(argc, argv) { ... }'.
 * the parameter types come from the prototype, in order.
 */
static int knrdef(void)
{
	char          name[SYMMAX];
	char          *savcurs = curs;
	struct func   *f;
	struct fnsig  *sig;
	char          parms[KNRMAX][SYMMAX];
	int           nparms = 0;
	int           framesave;

	readident(name, sizeof(name));
	f = funclookup(name);
	if (!f) {
		curs = savcurs;
		return 0;
	}

	if (f->func_defined)
		error("redefinition of function '%s'", name);
	if (f->func_ty->sty_kind != TYFUNC)
		error("'%s' is not a function", name);

	skipws();
	if (*curs != '(') {
		curs = savcurs;
		return 0;
	}
	advcurs(1); /* the '(' */

	sig = f->func_ty->sty_sig;
	if (!sig)
		error("'%s' has no prototype to take its parameters from", name);

	skipws();
	if (*curs != ')') {
		for (;;) {
			if (nparms == KNRMAX)
				error("too many parameters in '%s'", name);
			readident(parms[nparms], sizeof(parms[nparms]));
			nparms++;
			skipws();
			if (*curs == ',') {
				advcurs(1);
				continue;
			}
			if (*curs == ')') break;
			error("expected ',' or ')' in parameter list");
		}
	}
	advcurs(1); /* the ')' */

	skipws();
	if (*curs != '{')
		error("a function body was expected after '%s'", name);

	if (nparms != sig->fs_nargs)
		error("number of arguments doesn't match the prototype of '%s'",
		      name);

	framesave = symsave();
	for (int i = 0; i < nparms; i++)
		symadd(parms[i], depth + 1, sig->fs_args[i], SCLOCAL);

	fndecl(name, f->func_ty, framesave);
	symdrop(depth + 1);
	return 1;
}

void prog(void)
{
	skipws();

	while (*curs != '\0') {
		const struct keyword *kw = peekword();

		/*
		 * qualifiers (volatile, restrict, ...) are swallowed wherever
		 * they appear, file scope included, and left no trace.
		 */
		if (kw && kw->kw_func == doignored) {
			stmt(STMT);
			skipws();
			continue;
		}

		/*
		 * only declarations and function definitions may live at file
		 * scope; an expression statement there is invalid C and would
		 * become code no caller ever reaches.
		 */
		if (!kw || (kw->kw_func != doty && kw->kw_func != doextern)) {
			if (knrdef()) continue;
			error("expected a declaration at file scope");
		}

		stmt(STMT);
		skipws();
	}
}

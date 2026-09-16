#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include "cc.h"
#include "emit.h"
#include "ident.h"
#include "keywords.h"
#include "parse.h"
#include "util.h"

struct lval lval;

struct symty *expr_ty;

/* a dereference deferred because a postfix operator binds first */
static int derefpend;

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
		if (dst->sty_base->sty_size == 0 || src->sty_base->sty_size == 0)
			return;

		if (!tyeq(dst->sty_base, src->sty_base))
			error("assignment from incompatible pointer type");
		return;
	}

	if (dst->sty_signed != src->sty_signed && src->sty_size >= dst->sty_size)
		warn("implicit conversion changes signedness");

	if (src->sty_size > dst->sty_size)
		warn("implicit conversion loses %d bits of precision",
		     (src->sty_size - dst->sty_size) * 8);
}

void decl(struct symty *ty)
{
	int remaining = 0;

	do {
		struct sym     *s;
		struct symty   *curty = ty;
		char            name[SYMMAX];
		int             dims[16];
		int             ndims = 0;
		int             parens = 0;

		/*
		 * without function declarators the parentheses of a declarator
		 * are transparent: 'int *(*a);' is just 'int **a;'.
		 * count opens, and close them after the array brackets.
		 */
		skipws();
		while (*curs == '*' || *curs == '(') {
			if (*curs == '*') {
				curty = mkptr(curty);
				advcurs(1);
			} else {
				parens++;
				advcurs(1);
			}
		}

		readident(name, sizeof(name));

		/*
		 * 'int a[2][3]' declares an array of 2 arrays of 3 ints.
		 * the bracket next to the name is the outer one, so the
		 * dimensions wrap the base type from the innermost back.
		 */
		for (;;) {
			char              *end;
			unsigned long long len;

			skipws();
			if (*curs != '[') break;

			advcurs(1);
			errno = 0;
			len   = strtoull(curs, &end, 0);
			if (errno || end == curs || len == 0)
				error("invalid array length");
			curs = end;
			if (ndims == countof(dims))
				error("too many array dimensions");
			dims[ndims++] = (int)len;

			skipws();
			if (*curs != ']') error("expected ']'");
			advcurs(1);
		}

		for (int i = ndims - 1; i >= 0; --i)
			curty = mkarray(curty, dims[i]);

		s = symadd(name, depth, curty);

		/*
		 * a zero-sized type is the void type: it may only show up
		 * behind a pointer ('void *'), never as a standalone object.
		 */
		if (stysize(curty) == 0)
			error("variable '%s' cannot be void", s->sym_name);

		skipws();

		while (parens-- > 0) {
			skipws();
			if (*curs != ')') error("unbalanced '(' in declarator");
			advcurs(1);
		}

		remaining = 0;

		if (*curs == '=') {
			if (curty->sty_kind == TYARR)
				error("array initializer not yet supported");
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
			lval.lval_off  = s->sym_off;
			lval.lval_kind = STACK;
			lval.lval_ty   = s->sym_ty;
			store(lval);
		}

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
	unsigned long long val;
	char *end;
	int base;

	skipws();
	/* future note: we will need check for -- and ++ before this */
	un = unopundercurs();
	if (un) {
		advcurs(un->un_slen);
		factor();

		if (un->un_assoc == OPASSOCR) {
			struct lval l = lval;

			if (un->un_ptr == DEPTR) {
				if (l.lval_ty->sty_kind != TYPTR)
					error("cannot dereference non-pointer");

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
			} else if (l.lval_off == NONE) {
				error("l-value required for unary '%s' operand",
				      un->un_str);
			}

			if (un->un_ptr == GENPTR) l.lval_ty = mkptr(l.lval_ty);

			lval.lval_kind = un->un_genlval;
			lval.lval_off  = NONE;
			lval.lval_ty   = l.lval_ty;

			runemit(un->un_emit, un->un_tpl, l);
			return;
		}

		runemit(un->un_emit, un->un_tpl, lval);
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
			while (*curs == '*') {
				cty = mkptr(cty);
				advcurs(1);
			}
			skipws();
			if (*curs != ')') error("expected ')' after cast");
			advcurs(1);

			factor();
			l = lval;
			if (l.lval_kind == REGIS) deptr(l);
			cast(cty);
			lval.lval_kind = NONE;
			lval.lval_off  = NONE;
			lval.lval_ty   = cty;
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

	if (isalpha(*curs)) {
		struct sym *s  = readsym(-1);

		/*
		 * outside an initializer an array has no value: using its
		 * name decays it to a pointer to its first element.
		 */
		if (s->sym_ty->sty_kind == TYARR) {
			printf("	lea %d(%%rbp), %%rax\n", s->sym_off);
			lval.lval_kind = NONE;
			lval.lval_off  = s->sym_off;
			lval.lval_ty   = mkptr(s->sym_ty->sty_base);
			return;
		}

		lval.lval_kind = STACK;
		lval.lval_off  = s->sym_off;
		lval.lval_ty   = s->sym_ty;
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

	retval(val);
	lval.lval_kind = NONE;
	lval.lval_ty   = expr_ty->sty_kind == TYPTR ? defty : inferty(val);
}

void pointarith(const struct operator *op, struct symty *lhsty, struct symty *rhsty, int iscompound)
{
	int islhsptr = lhsty->sty_kind == TYPTR;
	int isrhsptr = rhsty->sty_kind == TYPTR;

	/* the two pointer flags add up to a single case number */
	if (islhsptr + isrhsptr == 2) error("both operands are pointers");
	if (islhsptr + isrhsptr == 0) return;
	if (iscompound && isrhsptr && !islhsptr)
		error("assignment makes integer from pointer without a cast");
	if (op->op_ptr == NOPTR)
		error("invalid pointer operation");

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
	if (!derefpend)
		return;

	if (lv->lval_ty->sty_kind != TYPTR)
		error("cannot dereference non-pointer");
	load(*lv);
	lv->lval_ty = lv->lval_ty->sty_base;
	derefpend = 0;
}

void expr(int min_prec)
{
	struct symty *saved = expr_ty;
	int           savedep = derefpend;

	derefpend = 0;

	factor();

	for (;;) {
		const struct operator *op = opundercurs();
		struct symty           *lhs_ty;

		if (!op || op->op_precedence < min_prec) break;
		advcurs(op->op_slen);

		if (op->op_assoc == OPASSOCR) {
			struct lval l = lval;
			if (l.lval_kind == NONE)
				error("assignment without an l-value.");
			derefvalue(&l);
			if (l.lval_kind == REGIS) regispre();

			if (op->op_assign != ASMOD) {
				expr_ty = l.lval_ty;
				expr(op->op_precedence);
			}

			if (l.lval_kind == REGIS) regispost();

			if (op->op_assign != ASMOD) {
				if (op->op_assign == ASTORE || (l.lval_ty->sty_kind != TYPTR && lval.lval_ty->sty_kind != TYPTR))
					tyassign(l.lval_ty, lval.lval_ty);

				if (op->op_assign != ASTORE) pointarith(op, l.lval_ty, lval.lval_ty, 1);
			}

			runemit(op->op_emit, op->op_tpl, l);

			lval.lval_kind = NONE;
			lval.lval_ty   = l.lval_ty;
			continue;
		}

		/*
		 * a register l-value is a pending dereference: the postfix
		 * '[' keeps the address, anything that follows needs the value.
		 */
		if (lval.lval_kind == REGIS && !op->op_postfix) {
			derefvalue(&lval);
			deptr(lval);
			lval.lval_kind = NONE;
		}

		/*
		 * If it is left-associated, overwrite the l-value.
		 */
		lhs_ty         = lval.lval_ty;
		lval.lval_kind = lval.lval_off = NONE;

		printf("	push %%rax\n");
		expr_ty = lhs_ty;
		expr(op->op_postfix ? -1 : op->op_precedence + 1);
		printf("	pop %%rcx\n");

		printf("	xchg %%rax, %%rcx\n");

		if (op->op_ptr == PTRARITH || op->op_ptr == NOPTR)
			pointarith(op, lhs_ty, lval.lval_ty, 0);

		lval.lval_ty = lhs_ty;
		runemit(op->op_emit, op->op_tpl, lval);
	}

	if (lval.lval_kind == REGIS) {
		derefvalue(&lval);
		deptr(lval);
		lval.lval_kind = NONE;
	}

	derefpend = savedep;
	expr_ty = saved;
}

void stmt(int mode)
{
	skipws();

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
		char name[1024];
		char              *savcurs      = curs;
		const struct keyword *kw = readword(name, sizeof(name));

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
		if (*curs == ':') {
			advcurs(1);
			printf(".L_lbl_%s:\n", name);
			stmt(STMT);
			return;
		}

		curs = savcurs;
	}

	expr_ty = defty;
	expr(-1);
	skipws();
	if (*curs != ';') {
		if (*curs == '\0' || iscntrl(*curs)) error("expected ';'");
		error("trailing characters '%c'", *curs++);
	}

	advcurs(1);
}

void prog(void)
{
	skipws();

	while (*curs != '\0') {
		stmt(STMT);
		skipws();
	}
}

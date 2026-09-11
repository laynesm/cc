#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cc.h"
#include "emit.h"
#include "ident.h"
#include "keywords.h"
#include "parse.h"
#include "util.h"

static struct lval lval;

/*
 * expression results that are not backed by a symbol
 * (numbers, arithmetics, unaries) degrade to this type.
 */
const struct symty defty = {
	.sty_signed = 1,
	.sty_isptr  = 0,
	.sty_size   = 8,
};

struct symty expr_ty;

/*
 * basic assignment type checking.
 * pointers are strict, scalars only warn.
 */
static void tyassign(struct symty *dst, struct symty src)
{
	if (dst->sty_isptr || src.sty_isptr) {
		if (!src.sty_isptr)
			error("assignment makes pointer from integer without a "
			      "cast");
		if (!dst->sty_isptr)
			error("assignment makes integer from pointer without a "
			      "cast");
		if (dst->sty_isptr != src.sty_isptr)
			error("assignment from incompatible pointer type");
		return;
	}

	if (dst->sty_signed != src.sty_signed && src.sty_size >= dst->sty_size)
		warn("implicit conversion changes signedness");

	if (src.sty_size > dst->sty_size)
		warn("implicit conversion loses %d bits of precision",
		     (src.sty_size - dst->sty_size) * 8);
}

void decl(int size, int sign, int isptr)
{
	int remaining = 0;

	do {
		struct sym *s = addsym(depth, size, sign, isptr);
		skipws();

		remaining = 0;

		if (*curs == '=') {
			advcurs(1);
			expr_ty = s->sym_ty;
			expr(-1);
			tyassign(&s->sym_ty, lval.lval_ty);
		} else {
			printf("\txor %%rax,%%rax\n");
		}

		lval.lval_off  = s->sym_off;
		lval.lval_kind = STACK;
		lval.lval_ty   = s->sym_ty;
		store(lval);

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

	char *end;
	long  val;

	skipws();
	/* future note: we will need check for -- and ++ before this */
	un = unopundercurs();
	if (un) {
		advcurs(1);
		factor();

		if (un->un_assoc == OPASSOCR) {
			struct lval l = lval;

			if (l.lval_off == NONE)
				error("l-value required for unary '%c' operand",
				      un->un_ch);

			if (un->un_ch == '*') {
				if (l.lval_ty.sty_isptr == 0)
					error("cannot dereference non-pointer");
				l.lval_ty.sty_isptr--;
			}

			if (un->un_ch == '&') l.lval_ty.sty_isptr++;

			lval.lval_kind = un->un_genlval;
			lval.lval_off  = NONE;
			lval.lval_ty   = l.lval_ty;

			un->un_emit(l);
			return;
		}

		un->un_emit(lval);
		return;
	}

	if (*curs == '(') {
		advcurs(1);
		expr(-1);
		skipws();
		if (*curs != ')') error("expected )");
		advcurs(1);
		return;
	}

	if (isalpha(*curs)) {
		struct sym *s  = readsym(-1);
		lval.lval_kind = STACK;
		lval.lval_off  = s->sym_off;
		lval.lval_ty   = s->sym_ty;
		load(lval);
		return;
	}

	val = strtol(curs, &end, 10);
	if (end == curs) error("expected expression");

	curs = end;
	skipws();

	retval(val);
	lval.lval_kind = NONE;
	lval.lval_ty   = expr_ty.sty_isptr ? defty : expr_ty;
}

void expr(int min_prec)
{
	struct symty saved = expr_ty;

	factor();

	for (;;) {
		const struct operator *op = opundercurs();
		struct symty           lhs_ty;

		if (!op || op->op_precedence < min_prec) break;
		advcurs(op->op_slen);

		if (op->op_assoc == OPASSOCR) {
			struct lval l = lval;
			if (l.lval_kind == NONE)
				error("assignment without an l-value.");
			if (l.lval_kind == REGIS) regispre();
			expr_ty = l.lval_ty;
			expr(op->op_precedence);
			if (l.lval_kind == REGIS) regispost();

			tyassign(&l.lval_ty, lval.lval_ty);

			op->op_emit(l);

			lval.lval_kind = NONE;
			lval.lval_ty   = l.lval_ty;
			continue;
		}

		/*
		 * If it is left-associated, overwrite the l-value.
		 */
		lhs_ty         = lval.lval_ty;
		lval.lval_kind = lval.lval_off = NONE;

		printf("	push %%rax\n");
		expr_ty = lhs_ty;
		expr(op->op_precedence + 1);
		printf("	pop %%rcx\n");

		printf("	xchg %%rax, %%rcx\n");

		if (op->op_emit == add || op->op_emit == sub) {
			int islhsptr = lhs_ty.sty_isptr;
			int isrhsptr = lval.lval_ty.sty_isptr;

			if (islhsptr && isrhsptr)
				error("invalid pointer arithmetic");

			if (islhsptr && !isrhsptr) {
				int sz = (lhs_ty.sty_isptr > 1)
				                 ? 8
				                 : lhs_ty.sty_size;
				if (sz > 1) printf("	imul $%d, %%rcx\n", sz);
			} else if (isrhsptr && !islhsptr) {
				int sz = (lval.lval_ty.sty_isptr > 1)
				                 ? 8
				                 : lhs_ty.sty_size;
				if (sz > 1) printf("	imul $%d, %%rax\n", sz);
			}
		}

		if (op->op_emit == mul || op->op_emit == idiv ||
		    op->op_emit == rem) {
			if (lhs_ty.sty_isptr > 0 || lval.lval_ty.sty_isptr > 0)
				error("invalid pointer operation");
		}

		lval.lval_ty = lhs_ty;
		op->op_emit(lval);
	}

	if (lval.lval_kind == REGIS) {
		deptr(lval);
		lval.lval_kind = NONE;
	}

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
			error("a declaration or an expression was expected here");
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
		const struct keyword *kw = readkeyword(0, -1);
		if (kw) {
			/*
			 * DECEXP only accepts declarations (a type) or plain
			 * expressions, so any other keyword is rejected.
			 */
			if (mode == DECEXP && kw->kw_func != doty)
				error("a declaration or an expression was expected "
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

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
		struct sym    *s;
		struct symty   *curty = ty;
		int parens = 0;

		/*
		 * without arrays and function declarators the parentheses
		 * of a declarator are transparent: 'int *(*a);' is just
		 * 'int **a;'. count opens, and close them after the name.
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

		s = addsym(curty);
		skipws();

		while (parens-- > 0) {
			skipws();
			if (*curs != ')') error("unbalanced '(' in declarator");
			advcurs(1);
		}

		remaining = 0;

		if (*curs == '=') {
			advcurs(1);
			expr_ty = s->sym_ty;
			expr(-1);
			tyassign(s->sym_ty, lval.lval_ty);
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

			if (l.lval_off == NONE)
				error("l-value required for unary '%s' operand",
				      un->un_str);

			if (un->un_ptr == DEPTR) {
				if (l.lval_ty->sty_kind != TYPTR)
					error("cannot dereference non-pointer");
				l.lval_ty = l.lval_ty->sty_base;
			}

			if (un->un_ptr == GENPTR) l.lval_ty = mkptr(l.lval_ty);

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
		const struct keyword *kw;
		struct symty         *cty;
		const char           *savcurs = curs;

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
		lval.lval_kind = STACK;
		lval.lval_off  = s->sym_off;
		lval.lval_ty   = s->sym_ty;
		load(lval);
		return;
	}

	errno = 0;
	if (*curs == '0') {
		base = -1;

		switch (tolower(curs[1])) {
		case 'x': base = 16; break;
		case 'd': base = 10; break;
		case 'b': base = 2; break;
		case 'o': base = 8; break;
		default: break;
		}

		if (base != -1) {
			if (!isdigit(curs[2])) error("incomplete literal");
			curs += 2;
		}

		if (base == -1) base = 8;
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

	struct symty *ptrty = islhsptr ? lhsty : rhsty;
	const char   *reg   = islhsptr && !iscompound ? "%rcx" : "%rax";
	int           sz    = ptrstep(ptrty);

	if (islhsptr && isrhsptr) error("both operands are pointers");
	if (!islhsptr && !isrhsptr) return;
	if (iscompound && isrhsptr && !islhsptr) error("assignment makes integer from pointer without a cast");

	if (op->op_ptr == NOPTR)
		error("invalid pointer operation");

	if (isrhsptr) *lhsty = *rhsty;
	if (sz > 1) printf("	imul $%d, %s\n", sz, reg);
}

void expr(int min_prec)
{
	struct symty *saved = expr_ty;

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
		expr(op->op_postfix ? -1 : op->op_precedence + 1);
		printf("	pop %%rcx\n");

		printf("	xchg %%rax, %%rcx\n");

		if (op->op_ptr == PTRARITH || op->op_ptr == NOPTR)
			pointarith(op, lhs_ty, lval.lval_ty, 0);

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
		const char *savcurs      = curs;
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

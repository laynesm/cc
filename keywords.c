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

static int lblcnt = 0;

/*
 * stack of the innermost loop's break/continue targets.
 * break jumps to the loop-end label, continue to the loop-top
 * label (for the increment step in 'for', the condition in
 * while/do-while).
 */
#define LOOPMAX 64 
static int brklbl[LOOPMAX];
static int cntlbl[LOOPMAX];
static int looppos = 0;

int newlbl(void)
{
	return lblcnt++;
}

static void looppush(int brk, int cnt)
{
	if (looppos == LOOPMAX) error("stop programming. you ashame yourself.");
	brklbl[looppos] = brk;
	cntlbl[looppos] = cnt;
	looppos++;
}

static void looppop(void)
{
	looppos--;
}

static int parse_cond(const char *father)
{
	int false_lbl = newlbl();

	skipws();
	if (*curs != '(') error("'(' expected after %s", father);
	advcurs(1);

	expr_ty = defty;
	expr(0);

	skipws();
	if (*curs != ')') error("missing ')' for %s", father);
	advcurs(1);

	materialize(&lval);
	cmp(0);
	jelbl(false_lbl);

	return false_lbl;
}

void doignored(const struct keyword *) {}

void dogoto(const struct keyword *)
{
	char name[1024];
	skipws();
	readident(name, sizeof(name));

	skipws();
	if (*curs != ';') error("expected ';' after goto");
	advcurs(1);

	printf("	jmp .L_lbl_%s\n", name);
}

/*
 * consumes the type keywords that follow an already-consumed first one
 * (given by 'first') and returns the type they describe. used by both
 * doty() (declarations) and the cast branch in parse.c: they share the
 * relation and sign rules, so a cast is just another direction of use.
 */
struct symty *parsety(int first)
{
	char buf[KWMAX];
	int   bits, longs, prio, size, sign, t;

	bits  = first;
	longs = (first == TYLONG);

	skipws();
	while (isalpha(*curs) || *curs == '_') {
		int                   id;
		const struct keyword *kw = peekword();

		if (!kw) break;

		/* the keyword table decides who is a type */
		if (kw->kw_func != doty) error("unexpected keyword");

		/* consume the peeked type keyword */
		kw = readword(buf, sizeof(buf));
		id = kw->kw_id;

		/* the type table decides who relates to whom */
		for (t = TYSIGNED; t <= TYVOID; t <<= 1)
			if ((bits & t) && !(tytbl[t].ty_relate & id))
				error("type '%s' does not relate to '%s'",
				      kwtbl[t].kw_str, kw->kw_str);

		if (id == TYLONG) {
			if (longs == 2)
				error("long long long is too much long");
			longs++;
		}

		bits |= id;
		skipws();
	}

	/* the winning type decides the size, the sign lives in the same data */
	prio = -1;
	size = 0;
	for (t = TYSIGNED; t <= TYVOID; t <<= 1)
		if ((bits & t) && tytbl[t].ty_prio > prio) {
			prio = tytbl[t].ty_prio;
			size = tytbl[t].ty_size;
			sign = tytbl[t].ty_signed;
		}

	/* an explicit sign always beats the type default */
	if (bits & TYSIGN)
		sign = tytbl[bits & TYSIGNED ? TYSIGNED : TYUNSIGNED].ty_signed;

	return sclty(size, sign);
}

void doty(const struct keyword *basety)
{
	struct symty *ty = parsety(basety->kw_id);
	char         *savcurs;

	/* doty operates on the scope-depth */
	depth--;

	skipws();
	savcurs = curs;

	/*
	 * a declaration may hide behind pointers and parenthesized
	 * declarators (int *p, int *(a), int (*fp)()):
	 * peek past them for the name, then let decl() scan again.
	 */
	while (*curs == '(' || *curs == '*') advcurs(1);

	if (isalpha(*curs) || *curs == '_' || *curs == '$') {
		curs = savcurs;
		decl(ty);
	} else {
		warn("cast not yet implemented");
	}

	/* settle depth back for the later symdrop */
	depth++;
	advcurs(1);
}

void doreturn(const struct keyword *)
{
	expr_ty = defty;
	expr(0);
	jmp(".L_ret_main");
}

void doif(const struct keyword *)
{
	int false_lbl = parse_cond("if");

	stmt(STMT); /* Parse the TRUE block */

	skipws();
	/*
	 * i like the function readkeyword
	 */
	if ((isalpha(*curs) || *curs == '_') && readkeyword(0, ELSE)) {
		int end_lbl = newlbl();

		/* If true block finishes, jump over the else block */
		jmplbl(end_lbl);

		/* Label for the else (false) block */
		idlbl(false_lbl);
		stmt(STMT); /* Parse the FALSE block */

		idlbl(end_lbl);
		return;
	}

	/* No else found, just mark the end of the true block */
	idlbl(false_lbl);
}

void dowhile(const struct keyword *)
{
	int start_lbl = newlbl();
	int false_lbl;

	idlbl(start_lbl);

	false_lbl = parse_cond("while");

	looppush(false_lbl, start_lbl);

	stmt(STMT); /* Loop body */

	/* Jump back to the condition check */
	jmplbl(start_lbl);
	idlbl(false_lbl);

	looppop();
}

void dofor(const struct keyword *)
{
	int   cond_lbl, end_lbl, cont_lbl;
	char *post_start, *post_end, *body_end;
	int   parens = 0;

	skipws();
	if (*curs != '(') error("'(' expected after for");
	advcurs(1);

	skipws();
	expr_ty = defty;
	stmt(DECEXP);
	skipws();

	cond_lbl = newlbl();
	end_lbl  = newlbl();
	cont_lbl = newlbl();

	idlbl(cond_lbl);
	skipws();

	if (*curs != ';') {
		expr_ty = defty;
		expr(0);
		materialize(&lval);
		cmp(0);
		jelbl(end_lbl);
	}

	if (*curs != ';') error("expected ';' in for cond");
	advcurs(1);

	post_start = curs;
	while ((*curs != ')' || parens > 0) && *curs != '\0') {
		parens += *curs == '(' ? 1 : *curs == ')' ? -1 : 0;
		curs++;
	}

	if (*curs != ')') error("missing ')' in for");
	
	post_end = curs;
	advcurs(1);

	looppush(end_lbl, cont_lbl);

	stmt(STMT);
	body_end = curs;

	idlbl(cont_lbl);

	curs = post_start;
	skipws();
	if (curs != post_end) {
		expr_ty = defty;
		expr(-1);
	}

	curs = body_end;

	jmplbl(cond_lbl);
	idlbl(end_lbl);

	looppop();
	symdrop(depth);
}

void dodowhile(const struct keyword *)
{
	int start_lbl = newlbl();
	int cont_lbl  = newlbl();
	int end_lbl   = newlbl();

	idlbl(start_lbl);

	looppush(end_lbl, cont_lbl);

	stmt(STMT);

	skipws();

	if (isalpha(*curs) || *curs == '_') {
		readkeyword(1, WHILE);
	} else {
		error("expected 'while' after do block");
	}

	/* parse cond? */
	skipws();
	if (*curs != '(') error("'(' expected after do-while");
	advcurs(1);

	idlbl(cont_lbl);
	expr_ty = defty;
	expr(0);
	skipws();
	if (*curs != ')') error("missing ')' for do-while");
	advcurs(1);
	if (*curs != ';') error("missing ';'");

	materialize(&lval);
	cmp(0);
	jnelbl(start_lbl);

	idlbl(end_lbl);

	looppop();
}

void doelse(const struct keyword *)
{
	error("orphan else");
}

void dobreak(const struct keyword *)
{
	if (looppos == 0) error("break outside of a loop");

	skipws();
	if (*curs != ';') error("expected ';' after break");
	advcurs(1);

	jmplbl(brklbl[looppos - 1]);
}

void docontinue(const struct keyword *)
{
	if (looppos == 0) error("continue outside of a loop");

	skipws();
	if (*curs != ';') error("expected ';' after continue");
	advcurs(1);

	jmplbl(cntlbl[looppos - 1]);
}

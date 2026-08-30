#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cc.h"
#include "emit.h"
#include "ident.h"
#include "parse.h"
#include "util.h"

static int lblcnt = 0;

static int newlbl(void)
{
	return lblcnt++;
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

	emit_cmp(0);
	emit_je_label(false_lbl);

	return false_lbl;
}

void doignored(struct keyword *){}

void doty(struct keyword *basety)
{
	char buf[KWMAX];

	int size, sign, sign_seen, bits, illong, seen_char, has_short, has_long;
	const struct type *ty = &tytbl[basety->kw_id];
	const char *savcurs = curs;

	/* doty operates on the scope-depth */
	depth--;

	illong    = 0;
	seen_char = (basety->kw_id == TYCHAR);
	has_short = (basety->kw_id == TYSHORT);
	has_long  = (basety->kw_id == TYLONG);
	sign      = ty->ty_signed;
	sign_seen = (basety->kw_id == TYSIGNED || basety->kw_id == TYUNSIGNED);
	bits      = basety->kw_id;

	if (basety->kw_id == TYCHAR)        size = 1;
	else if (basety->kw_id == TYSHORT)  size = 2;
	else if (basety->kw_id == TYLONG)   size = 8;
	else                                size = 4;

	skipws();
	while (isalpha(*curs) || *curs == '_') {
		const struct keyword *kw;
		
		savcurs = curs;
		kw = readword(buf, sizeof(buf));
		if (!kw) {
			/*
			 * we would like to read the identifier here to you know, do a variable declaration.
			 */
			curs = savcurs;
			savcurs = NULL;
			break;
		}

		if (kw->kw_id > 0 && (kw->kw_id & (kw->kw_id - 1)) != 0)
			error("unexpected keyword");
		
		if (kw->kw_id == TYCHAR) {
			seen_char = 1;
		} else if (kw->kw_id == TYSHORT) {
			has_short = 1;
			size = 2;
		} else if (kw->kw_id == TYLONG) {
			has_long = 1;
			size = 8;
		} else if (kw->kw_id == TYINT && !has_short && !has_long) {
			size = 4;
		}

		if (kw->kw_id == TYSIGNED || kw->kw_id == TYUNSIGNED) {
			sign = tytbl[kw->kw_id].ty_signed;
			sign_seen = 1;
		} else if (!sign_seen) {
			sign = tytbl[kw->kw_id].ty_signed;
		}
	
		if (kw->kw_id == TYLONG) {
			if (illong) error("long long long is too much long");
			if (bits & TYLONG) illong = 1;
		}
		
		for (size_t i = 0; i < sizeof(bits) * 8; ++i) {
			int idx = (1u << i);
			if ((bits & idx) == 0) continue;
			if ((tytbl[idx].ty_relate & kw->kw_id) == 0)
				error("type '%s' does not relate to '%s'", kwtbl[idx].kw_str, kw->kw_str);
		}


		bits |= kw->kw_id;
			
		skipws();
	}

	if (seen_char) size = 1;

	int isptr = 0;

	skipws();
	while (*curs == '*') {
		isptr++;
		advcurs(1);
	}

	if (savcurs == NULL || isptr) {
		decl(size, sign, isptr);
	} else {
		warn("cast not yet implemented");
	}

	/* settle depth back for the later symdrop */
	depth++;
	advcurs(1);
}

void doreturn(struct keyword *)
{
	expr_ty = defty;
	expr(0);
	emit_jmp(".L_ret_main");
}

void doif(struct keyword *)
{
	int false_lbl = parse_cond("if");

	stmt(); /* Parse the TRUE block */

	skipws();
	/* 
	 * i like the function readkeyword
	 */
	if ((isalpha(*curs) || *curs == '_') && readkeyword(0,ELSE)) {
		int end_lbl = newlbl();

		/* If true block finishes, jump over the else block */
		emit_jmp_label(end_lbl);

		/* Label for the else (false) block */
		emit_label_id(false_lbl);
		stmt(); /* Parse the FALSE block */

		emit_label_id(end_lbl);
		return;
	}

	/* No else found, just mark the end of the true block */
	emit_label_id(false_lbl);
}

void dowhile(struct keyword *)
{
	int start_lbl = newlbl();
	int false_lbl;

	emit_label_id(start_lbl);

	false_lbl = parse_cond("while");

	stmt(); /* Loop body */

	/* Jump back to the condition check */
	emit_jmp_label(start_lbl);
	emit_label_id(false_lbl);
}

void dofor(struct keyword *)
{
	error("for is not implemented yet");
}

void dodowhile(struct keyword *)
{
	int start_lbl = newlbl();
	int end_lbl   = newlbl();

	emit_label_id(start_lbl);

	stmt();

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
	expr_ty = defty;
	expr(0);
	skipws();
	if (*curs != ')') error("missing ')' for do-while");
	advcurs(1);
	if (*curs != ';') error("missing ';'");

	emit_cmp(0);
	emit_jne_label(start_lbl);

	emit_label_id(end_lbl);
}

void doelse(void)
{
	error("orphan else");
}

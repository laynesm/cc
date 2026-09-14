#include <stddef.h>
#include <string.h>

#include "cc.h"
#include "emit.h"
#include "keywords.h"
#include "util.h"

static struct symtab symtab = (struct symtab){0};

/*
 * next good steps here are:
 * break, continue and goto.
 */
const struct keyword kwtbl[] = {
	[RETURN]     = {"return", RETURN, doreturn},
	[IF]         = {"if", IF, doif},
	[ELSE]       = {"else", ELSE, doelse},
	[WHILE]      = {"while", WHILE, dowhile},
	[FOR]        = {"for", FOR, dofor},
	[DO]         = {"do", DO, dodowhile},
	[REGISTER]   = {"register", REGISTER, doignored},
	[VOLATILE]   = {"volatile", VOLATILE, doignored},
	[RESTRICT]   = {"restrict", RESTRICT, doignored},
	[TYINT]      = {"int", TYINT, doty},
	[TYSIGNED]   = {"signed", TYSIGNED, doty},
	[TYUNSIGNED] = {"unsigned", TYUNSIGNED, doty},
	[TYLONG]     = {"long", TYLONG, doty},
	[TYSHORT]    = {"short", TYSHORT, doty},
	[TYCHAR]     = {"char", TYCHAR, doty},
	[BREAK]      = {"break", BREAK, dobreak},
	[CONTINUE]   = {"continue", CONTINUE, docontinue},
	[GOTO]       = {"goto", GOTO, dogoto},
};

/*
 * Probably '=' should be a operator too.
 * it is already. am i dummy?
 */
const struct operator optbl[OPNUM] = {
	[OPSHLEQ]  = {"<<=", 3, -1, OPASSOCR, NOPTR,   ASCOMP, 0, bshleq},
	[OPSHREQ]  = {">>=", 3, -1, OPASSOCR, NOPTR,   ASCOMP, 0, bshreq},
	[OPEQ]     = {"==", 2, 0, OPASSOCL, PTRNONE,   ASNONE, 0, eq},
	[OPNEQ]    = {"!=", 2, 0, OPASSOCL, PTRNONE,   ASNONE, 0, ne},
	[OPLE]     = {"<=", 2, 1, OPASSOCL, PTRNONE,   ASNONE, 0, le},
	[OPGE]     = {">=", 2, 1, OPASSOCL, PTRNONE,   ASNONE, 0, ge},
	[OPSHL]    = {"<<", 2, 1, OPASSOCL, NOPTR,     ASNONE, 0, bshl},
	[OPSHR]    = {">>", 2, 1, OPASSOCL, NOPTR,     ASNONE, 0, bshr},
	[OPADDEQ]  = {"+=", 2, -1, OPASSOCR, PTRARITH, ASCOMP, 0, addeq},
	[OPSUBEQ]  = {"-=", 2, -1, OPASSOCR, PTRARITH, ASCOMP, 0, subeq},
	[OPMULEQ]  = {"*=", 2, -1, OPASSOCR, NOPTR,    ASCOMP, 0, muleq},
	[OPDIVEQ]  = {"/=", 2, -1, OPASSOCR, NOPTR,    ASCOMP, 0, diveq},
	[OPREMEQ]  = {"%=", 2, -1, OPASSOCR, NOPTR,    ASCOMP, 0, remeq},
	[OPINC]    = {"++", 2, -1, OPASSOCR, PTRARITH, ASMOD, 0, inc},
	[OPDEC]    = {"--", 2, -1, OPASSOCR, PTRARITH, ASMOD, 0, dec},
	[OPAND]    = {"&&", 2,  1, OPASSOCL, PTRNONE,  ASNONE, 0, and},
	[OPOR]     = {"||", 2,  1, OPASSOCL, PTRNONE,  ASNONE, 0, or},
	[OPBANDEQ] = {"&=", 2, -1, OPASSOCR, NOPTR,    ASCOMP, 0, bandeq},
	[OPBOREQ]  = {"|=", 2, -1, OPASSOCR, NOPTR,    ASCOMP, 0, boreq},
	[OPXOREQ]  = {"^=", 2, -1, OPASSOCR, NOPTR,    ASCOMP, 0, bxoreq},
	[OPLT]     = {"<",  1, 1, OPASSOCL, PTRNONE,   ASNONE, 0, lt},
	[OPGT]     = {">",  1, 1, OPASSOCL, PTRNONE,   ASNONE, 0, gt},
	[OPADD]    = {"+",  1, 0, OPASSOCL, PTRARITH,  ASNONE, 0, add},
	[OPSUB]    = {"-",  1, 0, OPASSOCL, PTRARITH,  ASNONE, 0, sub},
	[OPMUL]    = {"*",  1, 1, OPASSOCL, NOPTR,     ASNONE, 0, mul},
	[OPDIV]    = {"/",  1, 1, OPASSOCL, NOPTR,     ASNONE, 0, idiv},
	[OPREM]    = {"%",  1, 1, OPASSOCL, NOPTR,     ASNONE, 0, rem},
	[OPASSIGN] = {"=",  1, -1, OPASSOCR, PTRNONE,  ASTORE, 0, store},
	[OPIDX]    = {"[",  1, 2, OPASSOCL, PTRARITH,  ASNONE, 1, idx},
	[OPBAND]   = {"&",  1,  1, OPASSOCL, NOPTR,    ASNONE, 0, band},
	[OPBOR]    = {"|",  1,  1, OPASSOCL, NOPTR,    ASNONE, 0, bor},
	[OPXOR]    = {"^",  1,  1, OPASSOCL, NOPTR,    ASNONE, 0, bxor},
};

/*
 * Unary operators.
 * They should have association type too.
 */
const struct unary untbl[] = {
	{"++", 2, OPASSOCR, STACK, PTRARITH, ASNONE, preinc}, /* before '+' */
	{"--", 2, OPASSOCR, STACK, PTRARITH, ASNONE, predec},
	{"+",  1, OPASSOCL, NONE,  PTRNONE,  ASNONE, pos},
	{"-",  1, OPASSOCL, NONE,  PTRNONE,  ASNONE, neg},
	{"~",  1, OPASSOCL, NONE,  PTRNONE,  ASNONE, bnot},
	{"!",  1, OPASSOCL, NONE,  PTRNONE,  ASNONE, lnot},
	{"*",  1, OPASSOCR, REGIS, DEPTR,    ASNONE, pos}, /* ignored we lead with it using REGIS */
	{"&",  1, OPASSOCR, NONE,  GENPTR,   ASNONE, ptr},
};

/*
 * Type table.
 * Parsing C standard types might be tricky.
 * We turned it into easy with that.
 */
const struct type tytbl[] = {
	[TYCHAR]  = {1, 0, 4, TYSIGN},
	[TYSHORT] = {2, 1, 3, TYINT | TYSIGN},
	[TYINT]   = {4, 1, 1, TYSIGN | TYLONG | TYSHORT},
	[TYLONG]  = {8, 1, 3, TYINT | TYSIGN | TYLONG},

	[TYUNSIGNED] = {4, 0, 0, TYCHAR | TYINT | TYLONG | TYSHORT},
	[TYSIGNED]   = {4, 1, 0, TYCHAR | TYINT | TYLONG | TYSHORT},
};

const struct keyword *kwlookup(const char *name)
{
	for (size_t i = 0; i < countof(kwtbl); ++i) {
		if ((i == kwtbl[i].kw_id) && strcmp(kwtbl[i].kw_str, name) == 0)
			return &kwtbl[i];
	}

	return NULL;
}

const struct operator *opundercurs(void)
{
	for (size_t i = 0; i < countof(optbl); ++i) {
		if (strncmp(curs, optbl[i].op_str, optbl[i].op_slen) != 0)
			continue;
		return &optbl[i];
	}

	return NULL;
}

const struct unary *unopundercurs(void)
{
	for (size_t i = 0; i < countof(untbl); ++i) {
		if (strncmp(untbl[i].un_str, curs, untbl[i].un_slen) != 0) continue;
		return &untbl[i];
	}

	return NULL;
}

struct sym *symlookup(char *name, int scope)
{
	for (int i = symtab.tab_nsyms - 1; i >= 0; --i) {
		if ((scope == -1 || symtab.tab_syms[i].sym_scope == scope) &&
		    strcmp(symtab.tab_syms[i].sym_name, name) == 0)
			return &symtab.tab_syms[i];
	}

	return NULL;
}

struct sym *symadd(char *name, int scope, int size, int sign, int isptr)
{
	struct sym *s;

	/*
	 * shouldn't ever happen
	 */
	if (scope == -1) panic("symadd called on negative scope");

	if (symlookup(name, scope)) error("symbol '%s' already declared", name);

	/*
	 * realloc would be handy here
	 */
	if (symtab.tab_nsyms >= SYMTABMAX) error("too many variables");

	s = &symtab.tab_syms[symtab.tab_nsyms];
	symtab.tab_nsyms++;

	strncpy(s->sym_name, name, sizeof(s->sym_name) - 1);
	s->sym_name[sizeof(s->sym_name) - 1] = '\0';

	/*
	 * a pointer should be sized on eight bytes always.
	 * and the adresses should be aligned
	 */
	symtab.tab_stackoff -= isptr ? 8 : size;
	s->sym_off = symtab.tab_stackoff;

	if (isptr || size > 1) symtab.tab_stackoff &= ~((isptr ? 8 : size) - 1);

	s->sym_scope         = scope;
	s->sym_ty.sty_size   = size;
	s->sym_ty.sty_signed = sign;
	s->sym_ty.sty_isptr  = isptr;
	return s;
}

/*
 * you can only drop the current scope
 */
void symdrop(int scope)
{
	for (int i = symtab.tab_nsyms - 1; i >= 0; --i) {
		if (symtab.tab_syms[i].sym_scope != scope) break;
		symtab.tab_nsyms--;
	}
}

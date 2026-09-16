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
	[TYVOID]     = {"void", TYVOID, doty},
	[BREAK]      = {"break", BREAK, dobreak},
	[CONTINUE]   = {"continue", CONTINUE, docontinue},
	[GOTO]       = {"goto", GOTO, dogoto},
};

/*
 * Probably '=' should be a operator too.
 * it is already. am i dummy?
 */
/*
 * fixed-asm operators carry a template (op_tpl), printed by emit().
 * operators that need the live lvalue keep an emitter function (op_emit).
 */
const struct operator optbl[OPNUM] = {
	[OPSHLEQ]  = {"<<=", 3, -1, OPASSOCR, NOPTR,   ASCOMP, 0, bshleq, NULL},
	[OPSHREQ]  = {">>=", 3, -1, OPASSOCR, NOPTR,   ASCOMP, 0, bshreq, NULL},
	[OPEQ]     = {"==", 2, 0, OPASSOCL, PTRNONE,   ASNONE, 0, NULL, EMITEQ},
	[OPNEQ]    = {"!=", 2, 0, OPASSOCL, PTRNONE,   ASNONE, 0, NULL, EMITNE},
	[OPLE]     = {"<=", 2, 1, OPASSOCL, PTRNONE,   ASNONE, 0, NULL, EMITLE},
	[OPGE]     = {">=", 2, 1, OPASSOCL, PTRNONE,   ASNONE, 0, NULL, EMITGE},
	[OPSHL]    = {"<<", 2, 1, OPASSOCL, NOPTR,     ASNONE, 0, NULL, EMITBSHL},
	[OPSHR]    = {">>", 2, 1, OPASSOCL, NOPTR,     ASNONE, 0, NULL, EMITBSHR},
	[OPADDEQ]  = {"+=", 2, -1, OPASSOCR, PTRARITH, ASCOMP, 0, addeq, NULL},
	[OPSUBEQ]  = {"-=", 2, -1, OPASSOCR, PTRARITH, ASCOMP, 0, subeq, NULL},
	[OPMULEQ]  = {"*=", 2, -1, OPASSOCR, NOPTR,    ASCOMP, 0, muleq, NULL},
	[OPDIVEQ]  = {"/=", 2, -1, OPASSOCR, NOPTR,    ASCOMP, 0, diveq, NULL},
	[OPREMEQ]  = {"%=", 2, -1, OPASSOCR, NOPTR,    ASCOMP, 0, remeq, NULL},
	[OPINC]    = {"++", 2, -1, OPASSOCR, PTRARITH, ASMOD, 0, inc, NULL},
	[OPDEC]    = {"--", 2, -1, OPASSOCR, PTRARITH, ASMOD, 0, dec, NULL},
	[OPAND]    = {"&&", 2,  1, OPASSOCL, PTRNONE,  ASNONE, 0, NULL, EMITAND},
	[OPOR]     = {"||", 2,  1, OPASSOCL, PTRNONE,  ASNONE, 0, NULL, EMITOR},
	[OPBANDEQ] = {"&=", 2, -1, OPASSOCR, NOPTR,    ASCOMP, 0, bandeq, NULL},
	[OPBOREQ]  = {"|=", 2, -1, OPASSOCR, NOPTR,    ASCOMP, 0, boreq, NULL},
	[OPXOREQ]  = {"^=", 2, -1, OPASSOCR, NOPTR,    ASCOMP, 0, bxoreq, NULL},
	[OPLT]     = {"<",  1, 1, OPASSOCL, PTRNONE,   ASNONE, 0, NULL, EMITLT},
	[OPGT]     = {">",  1, 1, OPASSOCL, PTRNONE,   ASNONE, 0, NULL, EMITGT},
	[OPADD]    = {"+",  1, 0, OPASSOCL, PTRARITH,  ASNONE, 0, NULL, EMITADD},
	[OPSUB]    = {"-",  1, 0, OPASSOCL, PTRARITH,  ASNONE, 0, NULL, EMITSUB},
	[OPMUL]    = {"*",  1, 1, OPASSOCL, NOPTR,     ASNONE, 0, NULL, EMITMUL},
	[OPDIV]    = {"/",  1, 1, OPASSOCL, NOPTR,     ASNONE, 0, NULL, EMITIDIV},
	[OPREM]    = {"%",  1, 1, OPASSOCL, NOPTR,     ASNONE, 0, NULL, EMITREM},
	[OPASSIGN] = {"=",  1, -1, OPASSOCR, PTRNONE,  ASTORE, 0, store, NULL},
	[OPIDX]    = {"[",  1, 2, OPASSOCL, PTRARITH,  ASNONE, 1, idx, NULL},
	[OPBAND]   = {"&",  1,  1, OPASSOCL, NOPTR,    ASNONE, 0, NULL, EMITBAND},
	[OPBOR]    = {"|",  1,  1, OPASSOCL, NOPTR,    ASNONE, 0, NULL, EMITBOR},
	[OPXOR]    = {"^",  1,  1, OPASSOCL, NOPTR,    ASNONE, 0, NULL, EMITBXOR},
};

/*
 * Unary operators.
 * They should have association type too.
 */
const struct unary untbl[] = {
	{"++", 2, OPASSOCR, STACK, PTRARITH, ASNONE, inc,  NULL},
	{"--", 2, OPASSOCR, STACK, PTRARITH, ASNONE, dec,  NULL},
	{"+",  1, OPASSOCL, NONE,  PTRNONE,  ASNONE, NULL, EMITNOP},
	{"-",  1, OPASSOCL, NONE,  PTRNONE,  ASNONE, NULL, EMITNEG},
	{"~",  1, OPASSOCL, NONE,  PTRNONE,  ASNONE, NULL, EMITBNOT},
	{"!",  1, OPASSOCL, NONE,  PTRNONE,  ASNONE, NULL, EMITLNOT},
	{"*",  1, OPASSOCR, REGIS, DEPTR,    ASNONE, NULL, EMITNOP}, /* ignored, lead with REGIS */
	{"&",  1, OPASSOCR, NONE,  GENPTR,   ASNONE, ptr,  NULL},
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
	[TYVOID]  = {0, 0, 2, 0},

	[TYUNSIGNED] = {4, 0, 0, TYCHAR | TYINT | TYLONG | TYSHORT},
	[TYSIGNED]   = {4, 1, 0, TYCHAR | TYINT | TYLONG | TYSHORT},
};

const struct keyword *kwlookup(const char *name)
{
	for (int i = 0; i < (int)countof(kwtbl); ++i)
		if ((i == kwtbl[i].kw_id) && strcmp(kwtbl[i].kw_str, name) == 0)
			return &kwtbl[i];

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

int framesize(void)
{
	return -symtab.tab_stackoff;
}

struct sym *symadd(char *name, int scope, struct symty *ty)
{
	struct sym *s;
	int         size = stysize(ty);

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
	symtab.tab_stackoff -= size;
	s->sym_off = symtab.tab_stackoff;

	if (size > 1) symtab.tab_stackoff &= ~(size - 1);

	s->sym_scope = scope;
	s->sym_ty    = ty;
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

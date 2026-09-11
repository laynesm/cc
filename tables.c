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
};

/*
 * Probably '=' should be a operator too.
 */
const struct operator optbl[OPNUM] = {
	[OPEQ]     = {.op_str        = "==",
                      .op_slen       = 2,
                      .op_precedence = 0,
                      .op_emit       = eq},
	[OPNEQ]    = {.op_str        = "!=",
                      .op_slen       = 2,
                      .op_precedence = 0,
                      .op_emit       = ne},
	[OPLE]     = {.op_str        = "<=",
                      .op_slen       = 2,
                      .op_precedence = 1,
                      .op_emit       = le},
	[OPGE]     = {.op_str        = ">=",
                      .op_slen       = 2,
                      .op_precedence = 1,
                      .op_emit       = ge},
	[OPLT]     = {.op_str        = "<",
                      .op_slen       = 1,
                      .op_precedence = 1,
                      .op_emit       = lt},
	[OPGT]     = {.op_str        = ">",
                      .op_slen       = 1,
                      .op_precedence = 1,
                      .op_emit       = gt},
	[OPADD]    = {.op_str        = "+",
                      .op_slen       = 1,
                      .op_precedence = 0,
                      .op_emit       = add},
	[OPSUB]    = {.op_str        = "-",
                      .op_slen       = 1,
                      .op_precedence = 0,
                      .op_emit       = sub},
	[OPMUL]    = {.op_str        = "*",
                      .op_slen       = 1,
                      .op_precedence = 1,
                      .op_emit       = mul},
	[OPDIV]    = {.op_str        = "/",
                      .op_slen       = 1,
                      .op_precedence = 1,
                      .op_emit       = idiv},
	[OPREM]    = {.op_str        = "%",
                      .op_slen       = 1,
                      .op_precedence = 1,
                      .op_emit       = rem},
	[OPASSIGN] = {"=", 1, -1, OPASSOCR, store},
};

/*
 * Unary operators.
 * They should have association type too.
 */
const struct unary untbl[] = {
	{'+', OPASSOCL, NONE, pos},  {'-', OPASSOCL, NONE, neg},
	{'~', OPASSOCL, NONE, bnot}, {'!', OPASSOCL, NONE, lnot},
	{'*', OPASSOCR, REGIS, pos}, /* ignored we lead with it using REGIS */
	{'&', OPASSOCR, NONE, ptr},
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
		if (untbl[i].un_ch != *curs) continue;
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

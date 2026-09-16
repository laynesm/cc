#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "cc.h"
#include "ident.h"
#include "keywords.h"
#include "parse.h"
#include "type.h"
#include "util.h"

#define SUFFIXMAX 16

struct suff {
	int           dim;
	struct fnsig *sig;
};

char declname[SYMMAX];

static struct fnsig *fnpar(void);
static struct symty *tysuf(struct symty *);

/*
 * consumes the type keywords that follow an already-consumed first one
 * (given by 'first') and returns the type they describe. used by both
 * the declaration and cast paths: they share the relation and sign rules,
 * so a cast is just another direction of use.
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


/*
 * a '(' at the core of a declarator is a parenthesized declarator (* or
 * an identifier follows) or a function parameter list (a type keyword).
 */
static int isgroupstart(void)
{
	char                  *savcurs = curs;
	const struct keyword *kw;

	skipws();
	if (*curs != '(') return 0;
	advcurs(1); /* peek past the '(' */
	kw = peekword();
	curs = savcurs;
	return !kw || kw->kw_func != doty;
}

/*
 * the suffixes hanging off a name or a group. 'int a[2][3]' is an array
 * of 2 arrays of 3 ints, so the suffix nearest the core is the outermost
 * one: the collected list folds from the last entry back.
 */
static struct symty *tysuf(struct symty *ty)
{
	struct suff suffs[SUFFIXMAX];
	int         nsuff = 0;

	for (;;) {
		skipws();
		if (*curs == '[') {
			char              *end;
			unsigned long long len;

			advcurs(1);
			errno = 0;
			len   = strtoull(curs, &end, 0);
			if (errno || end == curs || len == 0)
				error("invalid array length");
			curs = end;
			skipws();
			if (*curs != ']') error("expected ']'");
			advcurs(1);

			if (nsuff == SUFFIXMAX)
				error("too many declarator suffixes");
			suffs[nsuff].sig = NULL;
			suffs[nsuff++].dim = (int)len;
		} else if (*curs == '(') {
			if (nsuff == SUFFIXMAX)
				error("too many declarator suffixes");
			suffs[nsuff].dim = 0;
			suffs[nsuff++].sig = fnpar();
		} else {
			break;
		}
	}

	for (int i = nsuff - 1; i >= 0; --i)
		ty = suffs[i].sig ? mkfunc(ty, suffs[i].sig)
		                  : mkarray(ty, suffs[i].dim);

	return ty;
}

/*
 * '(' params ')': each parameter is a mini declaration (type keywords
 * plus a declarator that may be nameless, as in 'int f(int)'). named
 * parameters become frame variables; a lone 'void' declares none.
 */
static struct fnsig *fnpar(void)
{
	struct fnsig *sig = calloc(1, sizeof(*sig));
	char          saved[SYMMAX];

	strcpy(saved, declname);
	advcurs(1); /* the '(' */

	for (;;) {
		char                  buf[KWMAX];
		const struct keyword *kw;
		struct symty         *pty;
		int                   isvoid;

		skipws();
		if (*curs == ')') break;

		kw = peekword();
		if (!kw || kw->kw_func != doty)
			error("expected a parameter type");
		kw = readword(buf, sizeof(buf));

		/* an abstract parameter must not inherit the outer name */
		declname[0] = '\0';
		pty = declarator(parsety(kw->kw_id));
		skipws();

		isvoid = stysize(pty) == 0 && declname[0] == '\0';
		if (isvoid && sig->fs_nargs)
			error("'void' must be the only parameter");

		if (declname[0])
			symadd(declname, depth, pty);

		if (!isvoid) {
			if (sig->fs_nargs == sig->fs_cap) {
				sig->fs_cap = sig->fs_cap ? sig->fs_cap * 2 : 4;
				sig->fs_args = realloc(sig->fs_args,
				                       sig->fs_cap * sizeof(*sig->fs_args));
			}
			sig->fs_args[sig->fs_nargs++] = pty;
		}

		skipws();
		if (*curs == ',') {
			advcurs(1);
			continue;
		}
		if (*curs == ')') break;
		error("expected ',' or ')' in parameter list");
	}

	advcurs(1); /* the ')' */
	strcpy(declname, saved);
	return sig;
}

/*
 * the declarators of a declaration follow the standard grammar:
 *     declarator        = pointers direct-declarator
 *     pointers          = '*' pointers | eps
 *     direct-declarator = identifier | '(' declarator ')'
 *                       | direct-declarator '[' dim ']'
 *                       | direct-declarator '(' params ')'
 * the type builds around the identifier, so a parenthesized group nests
 * *inside* whatever suffixes hang off it: 'int (*p)[3]' is ptr(arr(3,
 * int)), never arr(3,ptr(int)). the group's suffix run therefore folds
 * onto the base first, and the material inside the parentheses parses on
 * top of that wrapped base.
 */
struct symty *declarator(struct symty *ty)
{
	/* prefix pointers bind looser than any suffix or group */
	while (*curs == '*') { ty = mkptr(ty); advcurs(1); }

	skipws();
	if (*curs == '(' && isgroupstart()) {
		char *open  = curs;
		char *probe = curs + 1;
		char *after;
		int   parens = 1;

		/* the ')' that closes this parenthesized declarator */
		while (*probe && parens) {
			parens += *probe == '(';
			parens -= *probe == ')';
			probe++;
		}
		if (parens) error("unbalanced '(' in declarator");
		after = probe;

		/*
		 * the group's suffix run wraps the base type first, so the
		 * pointers inside the parentheses land outside the suffix:
		 * 'int (*p)[3]' ends up ptr(arr(3,int)).
		 */
		curs  = after;
		ty    = tysuf(ty);
		after = curs;

		curs = open + 1;
		ty   = declarator(ty);
		curs = after;
		return ty;
	}

	/* the deepest identifier owns the name of this declaration */
	if (isalpha(*curs) || *curs == '_' || *curs == '$') {
		readident(declname, sizeof(declname));
		return tysuf(ty);
	}

	/* an abstract declarator needs no name (casts, prototypes) */
	return tysuf(ty);
}

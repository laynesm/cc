#ifndef PARSE_H
#define PARSE_H

/*
 * is a static symbol, allocated from the typool at startup.
 */
extern struct symty *defty;
extern struct symty *expr_ty;

/*
 * subscript into optbl of the root operator of the last top-level
 * expression, or -1 for a bare l-value. set by expr(), checked by the
 * condition parsers to warn about 'if (x = y)'.
 */
extern int expr_rootop;

/* set by the 'extern' keyword, consumed by the next declaration */
extern int extdecl;

/* the function being parsed: where 'return' jumps and what it may return */
extern int           funcretlbl;
extern struct symty *funcretty;

/*
 * the labels a function reaches ('goto x') and the ones it defines
 * ('x:'). a goto may leap ahead of its label in the single pass, so the
 * undefined target only surfaces once the whole body has been parsed:
 * labcheck tests each use against the definitions at function end.
 */
#define LABMAX (1024)

struct lab {
	char lab_name[SYMMAX];
};

extern struct lab labdefs[LABMAX];
extern int        nlabdefs;
extern struct lab labuses[LABMAX];
extern int        nlabuses;

void labreset(void);
void labadddef(const char *);
void labadduse(const char *);
void labcheck(void);

void decl(struct symty *);
void factor(void);
void stmt(int);
void expr(int);
void prog(void);

#endif
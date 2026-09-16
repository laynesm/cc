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

void decl(struct symty *);
void factor(void);
void stmt(int);
void expr(int);
void prog(void);

#endif
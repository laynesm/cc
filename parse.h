#ifndef PARSE_H
#define PARSE_H

/*
 * is a static symbol, allocated from the typool at startup.
 */
extern struct symty *defty;
extern struct symty *expr_ty;

void decl(struct symty *);
void factor(void);
void stmt(int);
void expr(int);
void prog(void);

#endif
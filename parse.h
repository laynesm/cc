#ifndef PARSE_H
#define PARSE_H

extern struct symty expr_ty;
extern const struct symty defty;

void decl(int, int, int);
void factor(void);
void stmt(int);
void expr(int);
void prog(void);

#endif

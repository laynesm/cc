#ifndef UTIL_H
#define UTIL_H

#include "cc.h"

struct symty *sclty(int, int);
struct symty *mkptr(struct symty *);
struct symty *mkarray(struct symty *, int);
struct symty *mkfunc(struct symty *, struct fnsig *);
struct symty *inferty(unsigned long long);

void tyinit(void);

/* the floor of log2(n); n is a power of two, n >= 1        */
int sizlog(int);

/* width in bytes of a value of this type (pointers are 8)     */
int stysize(struct symty *);
int symalign(struct symty *);

/* how many bytes ++/-- advance for a symbol of this type      */
int ptrstep(struct symty *);

/* structural equality, since each use allocates a new slot     */
int tyeq(struct symty *, struct symty *);

void warn(char *,...);
void panic(char *,...);
void error(char *,...);
void skipws(void);
void advcurs(int);

#endif

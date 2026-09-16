#ifndef TYPE_H
#define TYPE_H

#include "cc.h"

/*
 * the identifier of the declarator currently being parsed. the deepest
 * one wins ('int (*p)[3]' and 'int f(int x)' both leave the outer name),
 * a pure abstract declarator leaves it empty for the caller to detect.
 */
extern char declname[SYMMAX];

/*
 * base type from the type keywords, shared by declarations and casts.
 */
struct symty *parsety(int);

/*
 * a full declarator: prefix pointers, parenthesized groups and the
 * array/function suffixes. returns the derived type and sets declname.
 */
struct symty *declarator(struct symty *);

#endif
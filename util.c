#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "util.h"

/*
 * a flat pool of compile-time types.
 * slots are stable across the whole compilation, so types can point
 * to each other (struct node { struct node *next; }) without hassle.
 * for the sources we compile today, 512 slots is plenty.
 */
#define TYPEMAX 512

static struct symty typool[TYPEMAX];
static int          typooln;

static struct symty *newsy(void)
{
	if (typooln == TYPEMAX) error("out of typool slots");
	return &typool[typooln++];
}

struct symty *sclty(int size, int sign)
{
	struct symty *ty = newsy();

	ty->sty_kind   = TYSCALR;
	ty->sty_size   = size;
	ty->sty_signed = sign;
	ty->sty_align  = size;
	ty->sty_base   = NULL;
	ty->sty_sig    = NULL;
	return ty;
}

struct symty *mkptr(struct symty *base)
{
	struct symty *ty = newsy();

	ty->sty_kind = TYPTR;
	ty->sty_size = 8;
	ty->sty_align = 8;
	ty->sty_base = base;
	return ty;
}

struct symty *mkarray(struct symty *base, int len)
{
	struct symty *ty = newsy();

	ty->sty_kind  = TYARR;
	ty->sty_size  = base->sty_size * len;
	ty->sty_align = base->sty_align;
	ty->sty_len   = len;
	ty->sty_base  = base;
	return ty;
}

int stysize(struct symty *ty)
{
	if (ty->sty_kind == TYPTR || ty->sty_kind == TYFUNC) return 8;
	return ty->sty_size;
}

int ptrstep(struct symty *ty)
{
	if (ty->sty_kind == TYPTR) return ty->sty_base->sty_size;
	return 1;
}

/*
 * each declaration allocates a fresh slot, so type identity must be
 * structural: two int* built at different moments are the same type.
 */
int tyeq(struct symty *a, struct symty *b)
{
	if (a == b) return 1;
	if (a->sty_kind != b->sty_kind) return 0;

	switch (a->sty_kind) {
	case TYSCALR:
		return a->sty_size == b->sty_size &&
		       a->sty_signed == b->sty_signed;
	case TYPTR:
		return tyeq(a->sty_base, b->sty_base);
	default:
		return 0;
	}
}

struct symty *inferty(unsigned long long val)
{
	if (val <= 127)
		return sclty(1, 1);
	if (val <= 255)
		return sclty(1, 0);
	if (val <= 32767)
		return sclty(2, 1);
	if (val <= 65535)
		return sclty(2, 0);
	if (val <= 2147483647)
		return sclty(4, 1);
	if (val <= 4294967295ULL)
		return sclty(4, 0);
	if (val <= 9223372036854775807ULL)
		return sclty(8, 1);

	return sclty(8, 0);
}

/*
 * the default type for expression results not backed by a symbol
 * (numbers, arithmetics, unaries). a canonical 64-bit scalar.
 */
struct symty *defty;

void tyinit(void)
{
	if (defty) return;
	defty = sclty(8, 1);
}

void warn(char *fmt, ...)
{
	va_list va_args;
	va_start(va_args, fmt);

	fprintf(stderr, "%s\n", line);
	fprintf(stderr, "%*s", (int)(curs - line), "");
	fprintf(stderr, "^ \n");

	if (curs - line < 20) fprintf(stderr, "%*s", (int)(curs - line), "");

	fprintf(stderr, "warning: ");
	vfprintf(stderr, fmt, va_args);
	fprintf(stderr, "\n");

	va_end(va_args);
}

void panic(char *fmt, ...)
{
	va_list va_args;
	va_start(va_args, fmt);
	fprintf(stderr, "PANIC: ");
	vfprintf(stderr, fmt, va_args);
	fprintf(stderr, "\n");
	va_end(va_args);
	abort();
}

/*
 * error message.
 * a probable enhancement here would be colored and more pretty gcc-like output
 */
void error(char *fmt, ...)
{
	va_list va_args;
	va_start(va_args, fmt);

	fprintf(stderr, "%s\n", line);
	fprintf(stderr, "%*s", (int)(curs - line), "");
	fprintf(stderr, "^ \n");

	/*
	 * Reasonable error position to align
	 */
	if (curs - line < 20) fprintf(stderr, "%*s", (int)(curs - line), "");

	fprintf(stderr, "error: ");
	vfprintf(stderr, fmt, va_args);
	fprintf(stderr, "\n");

	va_end(va_args);
	exit(EXIT_FAILURE);
}

void skipws(void)
{
	while (isspace(*curs)) curs++;
}

void advcurs(int n)
{
	skipws();
	curs += n;
	skipws();
}

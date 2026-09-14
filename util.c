#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "util.h"

struct symty inferty(unsigned long long val)
{
	struct symty ty = {.sty_isptr = 0, .sty_size = 8, .sty_signed = 1};

	if (val <= 127) {
		ty.sty_size   = 1;
		ty.sty_signed = 1;
	} else if (val <= 255) {
		ty.sty_size   = 1;
		ty.sty_signed = 0;
	} else if (val <= 32767) {
		ty.sty_size   = 2;
		ty.sty_signed = 1;
	} else if (val <= 65535) {
		ty.sty_size   = 2;
		ty.sty_signed = 0;
	} else if (val <= 2147483647) {
		ty.sty_size   = 4;
		ty.sty_signed = 1;
	} else if (val <= 4294967295ULL) {
		ty.sty_size   = 4;
		ty.sty_signed = 0;
	} else if (val <= 9223372036854775807ULL) {
		ty.sty_size   = 8;
		ty.sty_signed = 1;
	} else {
		ty.sty_size   = 8;
		ty.sty_signed = 0;
	}

	return ty;
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

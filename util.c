#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

#include "util.h"

void warn(char *fmt, ...)
{
	va_list va_args;
	va_start(va_args, fmt);

	fprintf(stderr, "%s\n", line);
	fprintf(stderr, "%*s", (int) (curs - line), "");
	fprintf(stderr, "^ \n");

	if (curs - line < 20)
	fprintf(stderr, "%*s", (int) (curs - line), "");

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
	fprintf(stderr, "%*s", (int) (curs - line), "");
	fprintf(stderr, "^ \n");

	/*
	* Reasonable error position to align
	*/
	if (curs - line < 20)
		fprintf(stderr, "%*s", (int) (curs - line), "");

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

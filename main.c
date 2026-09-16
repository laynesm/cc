#include <stdio.h>

#include "cc.h"
#include "keywords.h"
#include "parse.h"
#include "util.h"

char *line  = NULL;
char *curs  = NULL;
int   depth = 0;

int main(int argc, char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "usage: %s expr\n", argv[0]);
		return 1;
	}

	line = curs = argv[1];
	tyinit();

	/*
	 * every function now brings its own frame: 'main' must be declared
	 * explicitly with a body for the program to be usable.
	 */
	prog();

	skipws();
	if (*curs != '\0') error("unexpected trailing characters");
	return 0;
}

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cc.h"
#include "emit.h"
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
	globl("main");
	prologue();

	prog();

	skipws();
	if (*curs != '\0') error("unexpected trailing characters");

	lbl(".L_ret_main");
	epilogue();
	ret();
	return 0;
}

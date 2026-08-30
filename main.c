#include <stdio.h>
#include <stdlib.h> 
#include <ctype.h>
#include <stdarg.h>
#include <string.h>

#include "cc.h"
#include "emit.h"
#include "util.h"
#include "parse.h"

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
	emit_globl("main");
	emit_prologue();
	
	prog();
	
	skipws();
   	if (*curs != '\0')
		error("unexpected trailing characters");

	emit_label(".L_ret_main");
	emit_epilogue();
	emit_ret();
   	return 0;
}

#include <stdio.h>

#include "cc.h"
#include "emit.h"
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

	int setup;
	int body;

	line = curs = argv[1];
	globl("main");

	/*
	 * the frame size is only known once the declarations have been
	 * parsed, so the stack setup hangs off the end of the function.
	 */
	setup = newlbl();
	body  = newlbl();
	beginframe(setup, body);

	tyinit();
	prog();

	skipws();
	if (*curs != '\0') error("unexpected trailing characters");

	lbl(".L_ret_main");
	epilogue();
	ret();

	endframe(setup, body, framesize());
	return 0;
}

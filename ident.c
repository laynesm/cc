#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cc.h"
#include "ident.h"
#include "util.h"

struct sym *addsym(int lookupdepth, int size, int sign, int isptr)
{
	char name[SYMMAX];

	readident(name, sizeof(name));
	skipws();

	if (symlookup(name, lookupdepth))
		error("'%s' symbol already declared", name);

	return symadd(name, depth, size, sign, isptr);
}

struct sym *readsym(int lookupdepth)
{
	char       *savcurs = curs;
	struct sym *s;
	char        name[SYMMAX];

	readident(name, sizeof(name));
	skipws();

	s = symlookup(name, lookupdepth);
	if (s) return s;

	curs = savcurs;
	error("undefined usage of '%s'", name);
	return NULL;
}

const struct keyword *readkeyword(int forcefull, int which)
{
	char                  name[KWMAX];
	char                 *savcurs = curs;
	const struct keyword *kw      = readword(name, sizeof(name));

	if (kw && (which < 0 || kw->kw_id == which)) return kw;
	if (forcefull) {
		if (which == -1) error("expected keyword");
		error("expected '%s' keyword", kwtbl[which].kw_str);
	}

	curs = savcurs;
	return NULL;
}

void readident(char *buf, size_t len)
{
	const struct keyword *kw = readword(buf, len);
	if (kw) error("unexpected keyword '%s'", kw->kw_str);
}

const struct keyword *readword(char *buf, size_t len)
{
	char  *start;
	size_t size;

	skipws();
	start = curs;

	if (!isalpha(*curs) && *curs != '_' && *curs != '$')
		error("expected a keyword or identifier");
	curs++;

	while (isalnum(*curs) || *curs == '_' || *curs == '?' || *curs == '$')
		curs++;
	size = (size_t)(curs - start);

	if (size >= len) error("identifier is too big");

	strncpy(buf, start, size);
	buf[size] = '\0';

	// skipws();
	return kwlookup(buf);
}

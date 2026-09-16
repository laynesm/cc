#include <ctype.h>
#include <string.h>

#include "cc.h"
#include "ident.h"
#include "util.h"

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

/*
 * looks at the word under the cursor (readword-style) without consuming
 * it, and returns its keyword if any. never errors on non-alphabetic
 * input: digits start a literal, not a keyword candidate.
 */
const struct keyword *peekword(void)
{
	static char buf[KWMAX];
	char       *start;
	size_t      n;

	skipws();
	start = curs;
	if (!isalpha(*curs) && *curs != '_' && *curs != '$')
		return NULL;

	do curs++;
	while (isalnum(*curs) || *curs == '_' || *curs == '$' || *curs == '?');

	n = (size_t)(curs - start);
	if (n >= KWMAX) n = KWMAX - 1;
	memcpy(buf, start, n);
	buf[n] = '\0';

	curs = start;
	return kwlookup(buf);
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

	if (size >= 1024) error("word is too big");
	if (size >= len) error("identifier is too big");

	strncpy(buf, start, size);
	buf[size] = '\0';

	// skipws();
	return kwlookup(buf);
}

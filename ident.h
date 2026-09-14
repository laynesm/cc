#ifndef IDENT_H
#define IDENT_H

struct sym *addsym(struct symty *);

/*
 * reads a valid declared symbol
 */
struct sym *readsym(int); 

/*
 * helper for when the keywords were already consumed by stmt()
 */
void readident(char *, size_t);

/*
 * when keywords were not yet consumed
 */
const struct keyword *readword(char *, size_t);

/*
 * looks at the word under the cursor without consuming it
 */
const struct keyword *peekword(void);

/*
 * reads a keyword or gets the cursor back
 */
const struct keyword *readkeyword(int,int);

#endif

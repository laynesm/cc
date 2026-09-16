#ifndef KEYWORDS_H
#define KEYWORDS_H

int newlbl(void);

void dogoto(const struct keyword *);
void doignored(const struct keyword *);
void doty(const struct keyword *);
void doif(const struct keyword *);
void doelse(const struct keyword *);
void doreturn(const struct keyword *);
void dowhile(const struct keyword *);
void dofor(const struct keyword *);
void dodowhile(const struct keyword *);
void dobreak(const struct keyword *);
void docontinue(const struct keyword *);

#endif
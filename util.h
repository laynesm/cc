#ifndef UTIL_H
#define UTIL_H

#include "cc.h"

void warn(char *,...);
void panic(char *,...);
void error(char *,...); 
void skipws(void);
void advcurs(int);

#endif

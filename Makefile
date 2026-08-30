.POSIX:
.PHONY: clean all

CC = cc -g
CFLAGS = -Wall -Wextra -std=c99

OBJS = main.o parse.o emit.o tables.o util.o ident.o keywords.o

all: cc

cc: $(OBJS)
	$(CC) $(CFLAGS) -o cc $(OBJS)

main.o: main.c cc.h parse.h emit.h util.h
parse.o: parse.c parse.h cc.h emit.h util.h
emit.o: emit.c emit.h cc.h
tables.o: tables.c cc.h emit.h
util.o: util.c util.h cc.h

.c.o:
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f cc $(OBJS)

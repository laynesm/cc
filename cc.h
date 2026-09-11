#ifndef CC_H
#define CC_H

struct lval;

/* keyword maximum length */
#define KWMAX 12

/* helper macro */
#define countof(x) (sizeof(x)/sizeof(x[0]))

/* limits */
#define SYMMAX    (256)
#define SYMTABMAX (256)

/* association */
#define OPASSOCL (0)
#define OPASSOCR (1)

/* statement modes passed to stmt() */
#define STMT   (0)
#define DECEXP (1)

/* operations */
#define OPEQ     0
#define OPNEQ    1
#define OPLE     2
#define OPGE     3
#define OPLT     4
#define OPGT     5
#define OPADD    6 
#define OPSUB    7
#define OPMUL    8
#define OPDIV    9
#define OPREM    10
#define OPASSIGN 11
#define OPNUM    12

#define TYSIGNED   (1)
#define TYUNSIGNED (2)
#define TYSIGN     (TYSIGNED | TYUNSIGNED)
#define TYCHAR     (4)
#define TYSHORT    (8)
#define TYINT      (16)
#define TYLONG     (32)

/* keywords, always use non-bit perfect numbers */
#define RETURN   0
#define IF       3
#define ELSE     5
#define WHILE    6
#define FOR      7
#define DO       9
#define REGISTER 10
#define RESTRICT 11
#define VOLATILE 12
#define BREAK    13
#define CONTINUE 14

/*
 * representation of the C primitive types
 * struct/union and stuff will not be here.
 */
struct type {
	int         ty_size;      /* size in bytes */
	int         ty_signed;    /* is signed by default? */
	int         ty_prio;      /* who wins the size when types combine */
	int         ty_relate; /*
			        * 
				* relations are the modifiers appliable to the type,
				* the "most significant" one is choosen.
				* using this system we can lead with stuff like:
				* unsigned char a;
				* char unsigned a;
				* signed int a;
				* int long a;
				* long long a;
				* int long long a;
				* unsigned int long long a;
				* short signed int a;
				* all of the upper are valid C-cases that we need to treat.
				* The runtime type is different from this type. 
				* This is just for the static type table.
				*/
};

struct operator {
	const char *op_str;
	size_t      op_slen;
	int         op_precedence;

	int    op_assoc;
	void (*op_emit)(struct lval);
};

struct keyword {
	const char *kw_str;
	int         kw_id;
	void      (*kw_func)(struct keyword *);
};

struct unary {
	char   un_ch;
	int    un_assoc;
	int    un_genlval;
	void (*un_emit)(struct lval);
};

struct symty {
	int sty_signed;
	int sty_isptr;
	int sty_size;
};

/*
 * At this point, a symbol is only a stack position
 * but in the future, it will have a type associated with it.
 * and at this point it is also constituted of a single character.
 * I'd constitute a simple of a static string (256byte)
 */
struct sym {
	char         sym_name[SYMMAX];
	int          sym_off;
	int          sym_scope;
	struct symty sym_ty;
};

/*
 * the symbol table itself
 */
struct symtab {
	struct sym tab_syms[SYMTABMAX];
	int        tab_nsyms;
	int        tab_stackoff;
};

extern const struct type tytbl[];
extern const struct keyword kwtbl[];
extern const struct operator optbl[];
extern const struct unary untbl[];

extern char *line;
extern char *curs;
extern int   depth;

const struct unary *unopundercurs(void);
const struct operator *opundercurs(void);

struct sym *symlookup(char *, int);
struct sym *symadd(char *, int, int, int, int);
void symdrop(int);

const struct keyword *kwlookup(const char *); 

#endif

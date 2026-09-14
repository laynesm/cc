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

/* pointer interaction, driven from the tables themselves */
#define PTRNONE   0 /* pointer passes through untouched */
#define NOPTR     1 /* pointer operand is invalid */
#define GENPTR    2 /* produces a pointer */
#define DEPTR     3 /* consumes a pointer */
#define PTRARITH  4 /* pointer arithmetic scales by the pointee size */

/* how an operator assigns to an l-value */
#define ASNONE 0 /* does not assign */
#define ASCOMP 1 /* compound assignment, keeps the l-value */
#define ASTORE 2 /* plain assignment */
#define ASMOD  3 /* modifies in place, takes no operand: postfix ++ -- */

/* statement modes passed to stmt() */
#define STMT   (0)
#define DECEXP (1)

/*
 * operations.
 * the table is matched in this very order, so longer strings
 * always come first: a prefix operator (like '>') must never
 * shadow a longer one (like '>>=').
 */
#define OPSHLEQ  0  /* <<= */
#define OPSHREQ  1  /* >>= */
#define OPEQ     2  /* ==  */
#define OPNEQ    3  /* !=  */
#define OPLE     4  /* <=  */
#define OPGE     5  /* >=  */
#define OPSHL    6  /* <<  */
#define OPSHR    7  /* >>  */
#define OPADDEQ  8  /* +=  */
#define OPSUBEQ  9  /* -=  */
#define OPMULEQ  10 /* *=  */
#define OPDIVEQ  11 /* /=  */
#define OPREMEQ  12 /* %=  */
#define OPINC    13 /* ++  */
#define OPDEC    14 /* --  */
#define OPAND    15 /* &&  */
#define OPOR     16 /* ||  */
#define OPBANDEQ 17 /* &=  */
#define OPBOREQ  18 /* |=  */
#define OPXOREQ  19 /* ^=  */
#define OPLT     20 /* <   */
#define OPGT     21 /* >   */
#define OPADD    22 /* +   */
#define OPSUB    23 /* -   */
#define OPMUL    24 /* *   */
#define OPDIV    25 /* /   */
#define OPREM    26 /* %   */
#define OPASSIGN 27 /* =   */
#define OPIDX    28 /* [   */
#define OPBAND   29 /* &   */
#define OPBOR    30 /* |   */
#define OPXOR    31 /* ^   */
#define OPNUM    32
	
#define TYSIGNED   (1)
#define TYUNSIGNED (2)
#define TYSIGN     (TYSIGNED | TYUNSIGNED)
#define TYCHAR     (4)
#define TYSHORT    (8)
#define TYINT      (16)
#define TYLONG     (32)
#define TYVOID     (64)

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
#define GOTO     15

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
	int    op_ptr;
	int    op_assign;
	int    op_postfix;
	void (*op_emit)(struct lval);
};

struct keyword {
	const char *kw_str;
	int         kw_id;
	void      (*kw_func)(struct keyword *);
};

struct unary {
	char  *un_str;
	int    un_slen;
	int    un_assoc;
	int    un_genlval;
	int    un_ptr;
	int    un_assign;
	void (*un_emit)(struct lval);
};

/* runtime type kinds */
enum tykind {
	TYSCALR, /* scalar: char/short/int/long */
	TYPTR,   /* pointer to sty_base */
	TYARR,   /* array: sty_base is the element, sty_len the count */
	TYAGG,   /* struct/union, reserved */
	TYFUNC,  /* function, reserved */
};

/*
 * function signatures are only used once TYFUNC comes into play.
 * field is reserved from the start so the typool layout stays put.
 */
struct fnsig {
	struct symty  *fs_ret;
	struct symty **fs_args;
	int            fs_nargs;
	int            fs_cap;
};

/*
 * the runtime type. pointers, arrays and functions live on the
 * typool as stable slots, so types can reference each other
 * recursively (struct node { struct node *next; }).
 */
struct symty {
	int          sty_kind;
	int          sty_size;
	int          sty_signed;
	int          sty_align;
	int          sty_len;
	struct symty *sty_base;
	struct fnsig *sty_sig;
};

/*
 * At this point, a symbol is only a stack position
 * but in the future, it will have a type associated with it.
 * and at this point it is also constituted of a single character.
 * I'd constitute a simple of a static string (256byte)
 */
struct sym {
	char          sym_name[SYMMAX];
	int           sym_off;
	int           sym_scope;
	struct symty *sym_ty;
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
struct sym *symadd(char *, int, struct symty *);
void symdrop(int);

const struct keyword *kwlookup(const char *); 

#endif

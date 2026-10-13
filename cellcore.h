/* cellcore.h: pure types and prototypes (no system includes; include compat.h first). */

/* link-level namespace: every libcell.a symbol starts with cell; sources keep the short names */
#define styleinit cellstyleinit
#define styleresolve cellstyleresolve
#define styleeffective cellstyleeffective
#define stylemix cellstylemix
#define styleset cellstyleset
#define stylesetresolve cellstylesetresolve
#define stylefind cellstylefind
#define stylelookup cellstylelookup
#define parsecolour cellparsecolour
#define themeparse cellthemeparse
#define themebuiltin cellthemebuiltin
#define gridinit cellgridinit
#define gridfree cellgridfree
#define gridput cellgridput
#define gridputs cellgridputs
#define gridfill cellgridfill
#define gridclear cellgridclear
#define gridinvalidate cellgridinvalidate
#define griddiff cellgriddiff
#define gridcommit cellgridcommit
#define gridresize cellgridresize
#define gridscroll cellgridscroll
#define gridpoint cellgridpoint
#define utfdecode cellutfdecode
#define utfencode cellutfencode
#define widthcheck cellwidthcheck
#define wtabinit cellwtabinit
#define wtabpx cellwtabpx
#define runespan cellrunespan
#define eawcells celleawcells
#define eawcheck celleawcheck

enum {
	Aunder = 1,
	Arev = 2,
	MaxStyle = 256,
	MaxName = 31,
	MaxCells = 1<<20,
	Errlen = 128,
	Wtabsize = 1024
};

typedef struct Style Style;
struct Style {
	char name[MaxName+1];
	u32int fg, bg;		/* 0xRRGGBBFF */
	int attr;
	u32int sfg, sbg;	/* as written on the line */
	int has;		/* 1: fg set, 2: bg set */
	short mixa, mixb;	/* mix recipe: b's set fields over a's; -1: not a mix */
};

typedef struct Styletab Styletab;
struct Styletab {
	Style s[MaxStyle];
	int n;
};

typedef struct Gcell Gcell;
struct Gcell {
	Rune r;
	ushort style;
	uchar span;		/* 1 normal, 2 lead of a wide rune, 0 continuation (r == 0, lead's style) */
	uchar pad;		/* unpainted px right of the glyph in its span; < cw */
};

typedef struct Wslot Wslot;
struct Wslot {
	Rune key;
	short px;
};

typedef struct Wtab Wtab;
struct Wtab {
	int cw;
	int (*measure)(void*, Rune);
	void *arg;
	int nmeasure;
	Wslot e[Wtabsize];
};

typedef struct Grid Grid;
struct Grid {
	int rows, cols;
	Gcell *next, *shown;
	int force;
	Wtab *w;		/* font widths; nil: every rune one exact cell */
};

typedef struct Run Run;
struct Run {
	int row, col, n, style;	/* n: cells */
	int nr, pad;		/* nr: runes in r; pad: unpainted px after an inexact rune's glyph */
	u32int fg, bg;
	int attr;
	Rune *r;
};

void styleinit(Styletab*);
void styleresolve(Styletab*);
/* drawn colours: resolved fg/bg, swapped for reverse */
void styleeffective(Style*, u32int *fg, u32int *bg);
/* id, or -1 with err filled (bad id, table full); b == 0 or a == b gives a */
int stylemix(Styletab*, int a, int b, char *err, int nerr);
int styleset(Styletab*, char *name, int nname, u32int fg, u32int bg, int attr);
/* styleset + styleresolve; *existed: name was defined before (caller invalidates) */
int stylesetresolve(Styletab*, char *name, int nname, u32int fg, u32int bg, int attr, int *existed);
int stylefind(Styletab*, char *name, int nname);
int stylelookup(Styletab*, char *name);
int parsecolour(char *s, int n, u32int *c);
int themeparse(Styletab*, char *text, char *err, int nerr);
int themebuiltin(Styletab*, char *err, int nerr);

int gridinit(Grid*, int rows, int cols);
void gridfree(Grid*);
/* keeps the top-left overlap, blanks a cut pair, force = 1; -1 with the grid untouched */
int gridresize(Grid*, int rows, int cols);
int gridscroll(Grid*, int row, int nrows, int n, int *src, int *dst, int *cnt);	/* shifts next and shown; 1 and the blit rows, or 0: no blit */
/* window pixel offset to cell: 1 inside the grid; 0 in the margin or outside, row/col clamped */
int gridpoint(Grid*, int cw, int ch, int dx, int dy, int *row, int *col);
int gridput(Grid*, int row, int col, Rune, int style);
int gridputs(Grid*, int row, int col, char *utf8, int style);
void gridfill(Grid*, int row, int col, int nrows, int ncols, Rune, int style);
void gridclear(Grid*);
void gridinvalidate(Grid*);
int griddiff(Grid*, Styletab*, Run *out, Rune *rbuf, int *ncell);
/* shown = next with ids >= t->n stored as 0 (what was drawn), force = 0 */
void gridcommit(Grid*, Styletab*);
int utfdecode(char *s, Rune *r);
int utfencode(char *buf, Rune r);

/* cw px per cell; measure(arg, r) returns the advance in px; all slots empty */
void wtabinit(Wtab*, int cw, int (*measure)(void*, Rune), void *arg);
int eawcells(Rune);	/* East-Asian-Width cells: 2 for W/F; test oracle (eaw.c), not in libcell.a */
int eawcheck(void);	/* range count if the table is sorted and non-overlapping, else -1 */
/* cached advance in px; calls measure on a miss */
int wtabpx(Wtab*, Rune);
/* returns 1 or 2 cells; *out is the rune to store, *pad the unpainted px */
int runespan(Wtab*, Rune r, Rune *out, int *pad);

int widthcheck(char *font, int *w, int n, int *cw, char *msg, int nmsg);

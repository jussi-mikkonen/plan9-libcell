/* celldraw.h: private impure header of the libdraw backend.
 * Include compat.h, <draw.h>, cell.h and cellcore.h first. */

/* link-level namespace: every libcell.a symbol starts with cell; sources keep the short names */
#define fontcell cellfontcell
#define fontwtab cellfontwtab
#define colorimage cellcolorimage
#define colorfree cellcolorfree
#define colorlive cellcolorlive
#define colordrop cellcolordrop

enum {
	MaxColor = 512
};

typedef struct Ccolor Ccolor;
struct Ccolor {
	u32int key;		/* 0xRRGGBBFF */
	Image *img;
	int live;		/* its 'b' reached the server in a successful flushimage */
};

struct Cell {
	Font *font;		/* owned by the caller */
	int cw, ch;		/* cell size in pixels */
	Grid g;
	Wtab wt;		/* per-rune advances, cached; g.w points here */
	Styletab st;
	Run *run;		/* rows*cols capacity, scratch for griddiff */
	Rune *rbuf;
	Ccolor col[MaxColor];
	int ncol;
	int blitq;		/* a scroll draw is queued: flush must call flushimage even with no damage */
};

int fontcell(Font*, int *cw, int *ch);
void fontwtab(Wtab*, Font*, int cw);
Image* colorimage(Cell*, u32int key);
void colorfree(Cell*);
void colorlive(Cell*);
void colordrop(Cell*, int lost);

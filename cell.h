/*
 * cell.h: libcell public API, a cell grid of styled text drawn with libdraw.
 * Include <u.h>, <libc.h> and <draw.h> before this header.
 *
 * Documented in cell(2).
 */

#pragma lib "libcell.a"

typedef struct Cell Cell;
#pragma incomplete Cell

enum {
	Cdefault = 0		/* style id of the "default" style */
};

enum {					/* attr bits for cellsetstyle; same values as the theme's underline/reverse */
	Cunderline = 1,
	Creverse = 2
};

Cell*	cellopen(Font*);			/* nil and werrstr on failure; caller keeps the font */
void	cellclose(Cell*);
void	cellsize(Cell*, int *rows, int *cols);
void	cellpixsize(Cell*, int *w, int *h);	/* pixel size of one cell */
int	cellresize(Cell*);	/* call after getwindow on every resize or move; 0, or -1 and werrstr (grid kept); next flush redraws all */
void	cellscroll(Cell*, int row, int nrows, int n);	/* rows [row,row+nrows) by n, n > 0 up; one draw; exposed rows become default spaces for the caller to refill */
void	cellinvalidate(Cell*);	/* next flush redraws every cell and the margins */
int	cellstyle(Cell*, char *name);		/* built-ins: default keyword comment string error selection cursor (ids 0..6); unknown name: Cdefault */
int	cellput(Cell*, int row, int col, Rune, int style);	/* cells stored: 0, 1 or 2; controls stored as space, unrepresentable runes as U+FFFD */
int	cellputs(Cell*, int row, int col, char *utf8, int style);	/* cells stored; a wide rune counts 2, one that does not fit at the right edge becomes a space */
int	cellwidth(Cell*, Rune);	/* cells cellput would store for the rune: 1 or 2 (0 for a nil Cell); clipping at the grid edge aside */
int	cellpoint(Cell*, Point, int *row, int *col);	/* window pixel to cell: 1 inside the grid; 0 in the margin or outside, row/col clamped to the nearest cell */
void	cellfill(Cell*, int row, int col, int nrows, int ncols, Rune, int style);
void	cellclear(Cell*);
int	cellloadthemestr(Cell*, char *text);	/* merges by name over current styles (built-in first); named style replaced wholesale, unset fg/bg follow default; 0, or -1 and werrstr "theme:LINE: msg"; all or nothing */
int	cellloadtheme(Cell*, char *path);	/* read file (max 64 KB), then as above */
int	cellsetstyle(Cell*, char *name, ulong rgbfg, ulong rgbbg, int attr);	/* id, or -1 and werrstr; colours 0xRRGGBB; redefines an existing name in place */
int	cellmix(Cell*, int a, int b);		/* id whose b-set fields override a's; same id on repeat; b == Cdefault or a == b gives a; -1 and werrstr on a bad id or when full */
int	cellflush(Cell*);			/* cells redrawn (a wide rune counts 2), or -1 and werrstr */

/* grid.c: pure cell grid: storage, put/fill/clear with clipping, UTF-8. */
#include "compat.h"
#include "cellcore.h"

#define SENTINEL 0xFFFFFFFFu

int
gridinit(Grid *g, int rows, int cols)
{
	int n, i;

	memset(g, 0, sizeof *g);
	if(rows < 0)
		rows = 0;
	if(cols < 0)
		cols = 0;
	if(cols != 0 && rows > MaxCells/cols)
		return -1;
	n = rows*cols;
	g->next = czalloc((n > 0 ? n : 1) * sizeof(Gcell));
	g->shown = czalloc((n > 0 ? n : 1) * sizeof(Gcell));
	if(g->next == nil || g->shown == nil){
		free(g->next);
		free(g->shown);
		g->next = nil;
		g->shown = nil;
		return -1;
	}
	for(i = 0; i < n; i++){
		g->next[i].r = ' ';
		g->next[i].style = 0;
		g->next[i].span = 1;
		g->shown[i].r = SENTINEL;
		g->shown[i].span = 1;
	}
	g->rows = rows;
	g->cols = cols;
	g->force = 1;
	return 0;
}

void
gridfree(Grid *g)
{
	free(g->next);
	free(g->shown);
	memset(g, 0, sizeof *g);
}

static void
blank(Gcell *c, int style)
{
	c->r = ' ';
	c->style = (ushort)style;
	c->span = 1;
	c->pad = 0;
}

int
gridresize(Grid *g, int rows, int cols)
{
	Gcell *next, *shown;
	int n, i, j, mr, mc;

	if(rows < 0)
		rows = 0;
	if(cols < 0)
		cols = 0;
	if(cols != 0 && rows > MaxCells/cols)
		return -1;
	n = rows*cols;
	next = czalloc((n > 0 ? n : 1) * sizeof(Gcell));
	shown = czalloc((n > 0 ? n : 1) * sizeof(Gcell));
	if(next == nil || shown == nil){
		free(next);
		free(shown);
		return -1;
	}
	for(i = 0; i < n; i++){
		next[i].r = ' ';
		next[i].style = 0;
		next[i].span = 1;
		shown[i].r = SENTINEL;
		shown[i].span = 1;
	}
	mr = rows < g->rows ? rows : g->rows;
	mc = cols < g->cols ? cols : g->cols;
	for(i = 0; i < mr; i++){
		for(j = 0; j < mc; j++)
			next[i*cols + j] = g->next[i*g->cols + j];
		if(mc > 0 && next[i*cols + mc - 1].span == 2)
			blank(&next[i*cols + mc - 1], next[i*cols + mc - 1].style);
	}
	free(g->next);
	free(g->shown);
	g->next = next;
	g->shown = shown;
	g->rows = rows;
	g->cols = cols;
	g->force = 1;
	return 0;
}

/* the cell at col is about to be overwritten: blank the other half of its pair */
static void
breakpair(Grid *g, int row, int col)
{
	Gcell *c;

	c = &g->next[row*g->cols + col];
	if(c->span == 0 && col > 0)
		blank(c - 1, c[-1].style);
	else if(c->span == 2 && col + 1 < g->cols)
		blank(c + 1, c[1].style);
}

/* the single mutation point; returns cells written (1 or 2) */
static int
storecell(Grid *g, int row, int col, Rune out, int span, int pad, int style)
{
	Gcell *c;

	if(style < 0 || style >= MaxStyle)
		style = 0;
	if(span == 2 && col + 1 >= g->cols){
		out = ' ';
		span = 1;
		pad = 0;
	}
	breakpair(g, row, col);
	if(span == 2)
		breakpair(g, row, col + 1);
	c = &g->next[row*g->cols + col];
	c->r = out;
	c->style = (ushort)style;
	c->span = (uchar)span;
	c->pad = (uchar)pad;
	if(span == 2){
		c[1].r = 0;
		c[1].style = (ushort)style;
		c[1].span = 0;
		c[1].pad = 0;
	}
	return span == 2 ? 2 : 1;
}

static int
store(Grid *g, int row, int col, Rune r, int style)
{
	Rune out;
	int span, pad;

	span = runespan(g->w, r, &out, &pad);
	return storecell(g, row, col, out, span, pad, style);
}

int
gridput(Grid *g, int row, int col, Rune r, int style)
{
	if(row < 0 || row >= g->rows || col < 0 || col >= g->cols)
		return 0;
	return store(g, row, col, r, style);
}

int
gridputs(Grid *g, int row, int col, char *utf8, int style)
{
	Rune r, out;
	int n, stored, span, pad;

	if(utf8 == nil || row < 0 || row >= g->rows)
		return 0;
	stored = 0;
	while(*utf8 != '\0'){
		n = utfdecode(utf8, &r);
		utf8 += n;
		span = runespan(g->w, r, &out, &pad);
		if(col < 0){
			/* a wide rune cut at column -1 leaves a space in column 0 */
			if(col + span > 0 && g->cols > 0)
				stored += storecell(g, row, 0, ' ', 1, 0, style);
			col += span;
			continue;
		}
		if(col >= g->cols)
			break;
		n = storecell(g, row, col, out, span, pad, style);
		col += n;
		stored += n;
	}
	return stored;
}

void
gridfill(Grid *g, int row, int col, int nrows, int ncols, Rune r, int style)
{
	Rune out;
	int i, j, span, pad;

	if(nrows <= 0 || ncols <= 0)
		return;
	if(row < 0){
		if(row <= -nrows)
			return;
		nrows += row;
		row = 0;
	}
	if(col < 0){
		if(col <= -ncols)
			return;
		ncols += col;
		col = 0;
	}
	if(row >= g->rows || col >= g->cols)
		return;
	if(nrows > g->rows - row)
		nrows = g->rows - row;
	if(ncols > g->cols - col)
		ncols = g->cols - col;
	span = runespan(g->w, r, &out, &pad);
	for(i = 0; i < nrows; i++){
		for(j = 0; j < ncols; ){
			if(span == 2 && j + 1 < ncols)
				j += storecell(g, row + i, col + j, out, 2, pad, style);
			else{
				if(span == 2)
					storecell(g, row + i, col + j, ' ', 1, 0, style);
				else
					storecell(g, row + i, col + j, out, 1, pad, style);
				j++;
			}
		}
	}
}

void
gridclear(Grid *g)
{
	int i, n;

	n = g->rows*g->cols;
	for(i = 0; i < n; i++){
		g->next[i].r = ' ';
		g->next[i].style = 0;
		g->next[i].span = 1;
		g->next[i].pad = 0;
	}
}

void
gridinvalidate(Grid *g)
{
	g->force = 1;
}

/* continuation byte at s[i]? never looks past a NUL: NUL is not a continuation */
static int
cont(char *s, int i)
{
	return ((uchar)s[i] & 0xC0) == 0x80;
}

int
utfdecode(char *s, Rune *r)
{
	int c, c1;

	c = (uchar)s[0];
	if(c < 0x80){
		*r = (Rune)c;
		return 1;
	}
	if(c >= 0xC2 && c <= 0xDF){
		if(cont(s, 1)){
			*r = (Rune)(((c & 0x1F) << 6) | ((uchar)s[1] & 0x3F));
			return 2;
		}
	}else if(c >= 0xE0 && c <= 0xEF){
		c1 = (uchar)s[1];
		if(cont(s, 1) && (c != 0xE0 || c1 >= 0xA0) && (c != 0xED || c1 <= 0x9F) && cont(s, 2)){
			*r = (Rune)(((c & 0x0F) << 12) | ((c1 & 0x3F) << 6) | ((uchar)s[2] & 0x3F));
			return 3;
		}
	}else if(c >= 0xF0 && c <= 0xF4){
		c1 = (uchar)s[1];
		if(cont(s, 1) && (c != 0xF0 || c1 >= 0x90) && (c != 0xF4 || c1 <= 0x8F)
		&& cont(s, 2) && cont(s, 3)){
			*r = (Rune)(((c & 0x07) << 18) | ((c1 & 0x3F) << 12)
				| (((uchar)s[2] & 0x3F) << 6) | ((uchar)s[3] & 0x3F));
			return 4;
		}
	}
	*r = 0xFFFD;
	return 1;
}

int
utfencode(char *buf, Rune r)
{
	if(r > 0x10FFFF || (r >= 0xD800 && r <= 0xDFFF))
		r = 0xFFFD;
	if(r < 0x80){
		buf[0] = (char)r;
		return 1;
	}
	if(r < 0x800){
		buf[0] = (char)(0xC0 | (r >> 6));
		buf[1] = (char)(0x80 | (r & 0x3F));
		return 2;
	}
	if(r < 0x10000){
		buf[0] = (char)(0xE0 | (r >> 12));
		buf[1] = (char)(0x80 | ((r >> 6) & 0x3F));
		buf[2] = (char)(0x80 | (r & 0x3F));
		return 3;
	}
	buf[0] = (char)(0xF0 | (r >> 18));
	buf[1] = (char)(0x80 | ((r >> 12) & 0x3F));
	buf[2] = (char)(0x80 | ((r >> 6) & 0x3F));
	buf[3] = (char)(0x80 | (r & 0x3F));
	return 4;
}

int
gridscroll(Grid *g, int row, int nrows, int n, int *src, int *dst, int *cnt)
{
	int i, j, k, absn, cols;

	if(n == 0 || nrows <= 0 || g->cols == 0)
		return 0;
	if(row < 0){
		if(row <= -nrows)
			return 0;
		nrows += row;
		row = 0;
	}
	if(row >= g->rows)
		return 0;
	if(nrows > g->rows - row)
		nrows = g->rows - row;
	cols = g->cols;
	absn = n < 0 ? (n == -n ? nrows : -n) : n;
	if(absn >= nrows){
		for(i = row; i < row + nrows; i++){
			for(j = 0; j < cols; j++){
				blank(&g->next[i*cols + j], 0);
				g->shown[i*cols + j].r = SENTINEL;
				g->shown[i*cols + j].span = 1;
			}
		}
		return 0;
	}
	k = nrows - absn;
	if(n > 0){
		memmove(g->next + row*cols, g->next + (row + absn)*cols, k*cols*sizeof(Gcell));
		memmove(g->shown + row*cols, g->shown + (row + absn)*cols, k*cols*sizeof(Gcell));
		i = row + k;
	}else{
		memmove(g->next + (row + absn)*cols, g->next + row*cols, k*cols*sizeof(Gcell));
		memmove(g->shown + (row + absn)*cols, g->shown + row*cols, k*cols*sizeof(Gcell));
		i = row;
	}
	for(; absn > 0; absn--, i++){
		for(j = 0; j < cols; j++){
			blank(&g->next[i*cols + j], 0);
			g->shown[i*cols + j].r = SENTINEL;
			g->shown[i*cols + j].span = 1;
		}
	}
	if(g->force)
		return 0;
	if(n > 0){
		*src = row + n;
		*dst = row;
	}else{
		*src = row;
		*dst = row - n;
	}
	*cnt = k;
	return 1;
}

int
gridpoint(Grid *g, int cw, int ch, int dx, int dy, int *row, int *col)
{
	int inside, r, c;

	*row = 0;
	*col = 0;
	if(g->rows <= 0 || g->cols <= 0 || cw <= 0 || ch <= 0)
		return 0;
	inside = dx >= 0 && dy >= 0 && dx < g->cols*cw && dy < g->rows*ch;
	if(dx < 0)
		c = 0;
	else if(dx >= g->cols*cw)
		c = g->cols - 1;
	else
		c = dx/cw;
	if(dy < 0)
		r = 0;
	else if(dy >= g->rows*ch)
		r = g->rows - 1;
	else
		r = dy/ch;
	*row = r;
	*col = c;
	return inside;
}

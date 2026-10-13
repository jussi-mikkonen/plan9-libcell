/* cell.c: Cell handle and the thin public API over the pure grid. */
#include "compat.h"
#include <draw.h>
#include "cell.h"
#include "cellcore.h"
#include "celldraw.h"

Cell*
cellopen(Font *f)
{
	Cell *c;
	int rows, cols, cw, ch, n;
	char err[Errlen];

	if(f == nil){
		werrstr("cellopen: nil font");
		return nil;
	}
	if(display == nil || screen == nil){
		werrstr("cellopen: no window");
		return nil;
	}
	if(fontcell(f, &cw, &ch) < 0)
		return nil;
	rows = Dy(screen->r) / ch;
	cols = Dx(screen->r) / cw;
	c = mallocz(sizeof *c, 1);
	if(c == nil){
		werrstr("cellopen: out of memory");
		return nil;
	}
	c->font = f;
	c->cw = cw;
	c->ch = ch;
	styleinit(&c->st);
	if(themebuiltin(&c->st, err, sizeof err) < 0){
		werrstr("cellopen: %s", err);
		free(c);
		return nil;
	}
	if(gridinit(&c->g, rows, cols) < 0){
		werrstr("cellopen: grid too large or out of memory");
		free(c);
		return nil;
	}
	fontwtab(&c->wt, f, cw);
	c->g.w = &c->wt;
	n = c->g.rows * c->g.cols;
	if(n < 1)
		n = 1;
	c->run = mallocz(n * sizeof(Run), 1);
	c->rbuf = mallocz(n * sizeof(Rune), 1);
	if(c->run == nil || c->rbuf == nil){
		werrstr("cellopen: out of memory");
		free(c->run);
		free(c->rbuf);
		gridfree(&c->g);
		free(c);
		return nil;
	}
	return c;
}

void
cellclose(Cell *c)
{
	if(c == nil)
		return;
	colorfree(c);
	gridfree(&c->g);
	free(c->run);
	free(c->rbuf);
	free(c);
}

void
cellsize(Cell *c, int *rows, int *cols)
{
	*rows = c->g.rows;
	*cols = c->g.cols;
}

int
cellresize(Cell *c)
{
	Run *run;
	Rune *rbuf;
	int rows, cols, n;

	if(display == nil || screen == nil){
		gridinvalidate(&c->g);
		werrstr("cellresize: no window");
		return -1;
	}
	rows = Dy(screen->r) / c->ch;
	cols = Dx(screen->r) / c->cw;
	if(rows < 0)
		rows = 0;
	if(cols < 0)
		cols = 0;
	if(cols != 0 && rows > MaxCells/cols){
		gridinvalidate(&c->g);
		werrstr("cellresize: grid too large");
		return -1;
	}
	n = rows * cols;
	if(n < 1)
		n = 1;
	run = mallocz(n * sizeof(Run), 1);
	rbuf = mallocz(n * sizeof(Rune), 1);
	if(run == nil || rbuf == nil){
		free(run);
		free(rbuf);
		gridinvalidate(&c->g);
		werrstr("cellresize: out of memory");
		return -1;
	}
	if(gridresize(&c->g, rows, cols) < 0){
		free(run);
		free(rbuf);
		gridinvalidate(&c->g);
		werrstr("cellresize: out of memory");
		return -1;
	}
	free(c->run);
	free(c->rbuf);
	c->run = run;
	c->rbuf = rbuf;
	return 0;
}

void
cellscroll(Cell *c, int row, int nrows, int n)
{
	int src, dst, cnt, x0, y0;

	if(!gridscroll(&c->g, row, nrows, n, &src, &dst, &cnt))
		return;
	if(display == nil || screen == nil || Dx(screen->r) <= 0 || Dy(screen->r) <= 0){
		gridinvalidate(&c->g);
		return;
	}
	x0 = screen->r.min.x;
	y0 = screen->r.min.y;
	draw(screen, Rect(x0, y0 + dst*c->ch, x0 + c->g.cols*c->cw, y0 + (dst+cnt)*c->ch),
		screen, nil, Pt(x0, y0 + src*c->ch));
	c->blitq = 1;
}

void
cellinvalidate(Cell *c)
{
	gridinvalidate(&c->g);
}

void
cellpixsize(Cell *c, int *w, int *h)
{
	*w = c->cw;
	*h = c->ch;
}

int
cellstyle(Cell *c, char *name)
{
	return stylelookup(&c->st, name);
}

int
cellloadthemestr(Cell *c, char *text)
{
	char err[Errlen];

	if(text == nil){
		werrstr("theme: nil text");
		return -1;
	}
	if(themeparse(&c->st, text, err, sizeof err) < 0){
		werrstr("%s", err);
		return -1;
	}
	/* colours behind existing style ids may have changed */
	colorfree(c);	/* drop the old palette's images; rebuilt on next flush */
	gridinvalidate(&c->g);
	return 0;
}

int
cellsetstyle(Cell *c, char *name, ulong rgbfg, ulong rgbbg, int attr)
{
	int id, existed;

	if(name == nil){
		werrstr("cellsetstyle: nil name");
		return -1;
	}
	id = stylesetresolve(&c->st, name, strlen(name), ((u32int)(rgbfg & 0xFFFFFF) << 8) | 0xFF,
		((u32int)(rgbbg & 0xFFFFFF) << 8) | 0xFF, attr & (Cunderline|Creverse), &existed);
	if(id < 0){
		werrstr("cellsetstyle: bad name or too many styles");
		return -1;
	}
	if(existed){
		/* (rune, id) is unchanged, so the diff would never repaint these cells */
		colorfree(c);
		gridinvalidate(&c->g);
	}
	return id;
}

int
cellmix(Cell *c, int a, int b)
{
	char err[Errlen];
	int id;

	id = stylemix(&c->st, a, b, err, sizeof err);
	if(id < 0)
		werrstr("cellmix: %s", err);
	return id;
}

int
cellput(Cell *c, int row, int col, Rune r, int style)
{
	return gridput(&c->g, row, col, r, style);
}

int
cellwidth(Cell *c, Rune r)
{
	Rune out;
	int pad;

	if(c == nil)
		return 0;
	return runespan(&c->wt, r, &out, &pad);
}

int
cellpoint(Cell *c, Point p, int *row, int *col)
{
	if(screen == nil){
		*row = 0;
		*col = 0;
		return 0;
	}
	return gridpoint(&c->g, c->cw, c->ch, p.x - screen->r.min.x, p.y - screen->r.min.y, row, col);
}

int
cellputs(Cell *c, int row, int col, char *utf8, int style)
{
	return gridputs(&c->g, row, col, utf8, style);
}

void
cellfill(Cell *c, int row, int col, int nrows, int ncols, Rune r, int style)
{
	gridfill(&c->g, row, col, nrows, ncols, r, style);
}

void
cellclear(Cell *c)
{
	gridclear(&c->g);
}

/* dflush.c: cellflush, the run interpreter: one string draw per run, one flushimage.
 *
 * Known gap (T-01-07): libdraw flushes automatically when its command buffer fills;
 * a failure there is only reported on fd 2 ("stringbg:"). Only the result of the final
 * flushimage is observable here, and on -1 the whole grid is repainted next time.
 * A queued scroll blit (blitq) keeps flushimage on the path even with no damage.
 * A zero-size window draws nothing (force kept); a 0-row or 0-col grid paints only margins.
 * Runs carry cells (n) and runes (nr); an inexact rune is a run of its own,
 * positioned from its cell, and the rest of its span is filled with bg.
 * Reverse arrives pre-swapped in the run list; underline is a 1 px rect on the cell's
 * bottom row; margins are painted with default's bg on forced flushes; a failed flush
 * drops the colour cache with colordrop, which never sends 'f' for an image whose 'b'
 * was lost with the failed batch (CR-01).
 */
#include "compat.h"
#include <draw.h>
#include "cell.h"
#include "cellcore.h"
#include "celldraw.h"

int
cellflush(Cell *c)
{
	Run *u;
	Image *fg, *bg, *m;
	Point pt;
	Rectangle r;
	u32int mfg, mbg;
	int i, nrun, ncell, margins, x, y;

	if(display == nil || screen == nil){
		c->blitq = 0;
		gridinvalidate(&c->g);
		werrstr("cellflush: no window");
		return -1;
	}
	if(Dx(screen->r) <= 0 || Dy(screen->r) <= 0)
		return 0;
	margins = c->g.force;
	if(margins){
		styleeffective(&c->st.s[0], &mfg, &mbg);
		m = colorimage(c, mbg);
		if(m == nil)
			goto fail;
		r = screen->r;
		x = r.min.x + c->g.cols * c->cw;
		y = r.min.y + c->g.rows * c->ch;
		if(x < r.max.x)
			draw(screen, Rect(x, r.min.y, r.max.x, r.max.y), m, nil, ZP);
		if(y < r.max.y)
			draw(screen, Rect(r.min.x, y, x < r.max.x ? x : r.max.x, r.max.y), m, nil, ZP);
	}
	nrun = griddiff(&c->g, &c->st, c->run, c->rbuf, &ncell);
	if(nrun == 0 && !margins && !c->blitq)
		return 0;
	for(i = 0; i < nrun; i++){
		u = &c->run[i];
		fg = colorimage(c, u->fg);
		bg = colorimage(c, u->bg);
		if(fg == nil || bg == nil)
			goto fail;
		pt = addpt(screen->r.min, Pt(u->col * c->cw, u->row * c->ch));
		runestringnbg(screen, pt, fg, ZP, c->font, u->r, u->nr, bg, ZP);
		if(u->pad > 0)
			draw(screen, Rect(pt.x + u->n * c->cw - u->pad, pt.y, pt.x + u->n * c->cw, pt.y + c->ch), bg, nil, ZP);
		if(u->attr & Aunder)
			draw(screen, Rect(pt.x, pt.y + c->ch - 1, pt.x + u->n * c->cw, pt.y + c->ch), fg, nil, ZP);
	}
	c->blitq = 0;
	if(flushimage(display, 1) < 0){
		werrstr("cellflush: flushimage: %r");
		gridinvalidate(&c->g);
		colordrop(c, 1);
		return -1;
	}
	colorlive(c);
	gridcommit(&c->g, &c->st);
	return ncell;

fail:
	werrstr("cellflush: %r");
	gridinvalidate(&c->g);
	colordrop(c, 0);
	return -1;
}

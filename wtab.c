/* wtab.c: pure per-rune width cache and span decision. */
#include "compat.h"
#include "cellcore.h"

#define EMPTY 0xFFFFFFFFu

void
wtabinit(Wtab *w, int cw, int (*measure)(void*, Rune), void *arg)
{
	int i;

	w->cw = cw;
	w->measure = measure;
	w->arg = arg;
	w->nmeasure = 0;
	for(i = 0; i < Wtabsize; i++){
		w->e[i].key = EMPTY;
		w->e[i].px = 0;
	}
}

int
wtabpx(Wtab *w, Rune r)
{
	Wslot *e;
	int px;

	e = &w->e[r & (Wtabsize-1)];
	if(e->key == r)
		return e->px;
	px = w->measure(w->arg, r);
	w->nmeasure++;
	if(px < 0)
		px = 0;
	if(px > 32767)
		px = 32767;
	e->key = r;
	e->px = (short)px;
	return px;
}

/* cells for r: 1 or 2 with *pad set, or 0 when r cannot be drawn in 1..2 cells */
static int
measured(Wtab *w, Rune r, int *pad)
{
	int px, cells, p;

	px = wtabpx(w, r);
	if(px <= 0 || w->cw <= 0)
		return 0;
	cells = (px + w->cw - 1) / w->cw;
	if(cells > 2)
		return 0;
	p = cells*w->cw - px;
	if(p > 255)
		p = 255;
	*pad = p;
	return cells;
}

int
runespan(Wtab *w, Rune r, Rune *out, int *pad)
{
	int cells;

	*pad = 0;
	if(r < 0x20 || (r >= 0x7F && r < 0xA0)){
		*out = ' ';
		return 1;
	}
	if(r > 0x10FFFF || (r >= 0xD800 && r <= 0xDFFF))
		r = 0xFFFD;
	if(r < 0x7F || w == nil){
		*out = r;
		return 1;
	}
	cells = measured(w, r, pad);
	if(cells == 0 && r != 0xFFFD){
		r = 0xFFFD;
		cells = measured(w, r, pad);
	}
	if(cells == 0){
		*out = ' ';
		*pad = 0;
		return 1;
	}
	*out = r;
	return cells;
}

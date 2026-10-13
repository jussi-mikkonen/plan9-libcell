/* diff.c: pure damage computation. griddiff never mutates the grid;
 * gridcommit is called by the backend only after the draw succeeded.
 * Runs carry drawn (effective) colours: reverse is already swapped. */
#include "compat.h"
#include "cellcore.h"

/* would this cell be drawn differently from what is shown? */
static int
differs(Grid *g, Styletab *t, int i)
{
	Gcell *a, *b;
	int sid;

	a = &g->next[i];
	b = &g->shown[i];
	/* compare the id that would be drawn: see gridcommit */
	sid = a->style < t->n ? (int)a->style : 0;
	return g->force || a->r != b->r || sid != b->style || a->span != b->span;
}

/*
 * Runs count cells in n and runes in nr (a wide rune is two cells, one rune).
 * A run never starts on a continuation: the two halves of a pair are
 * repainted together, from the lead. An inexact rune (pad > 0) is a run of
 * its own. *ncell is the number of cells in all runs.
 */
int
griddiff(Grid *g, Styletab *t, Run *out, Rune *rbuf, int *ncell)
{
	Gcell *a;
	Run *cur;
	int r, c, i, nrun, ncells, nrunes, open, sid, changed;

	nrun = 0;
	ncells = 0;
	nrunes = 0;
	cur = nil;
	for(r = 0; r < g->rows; r++){
		open = 0;
		for(c = 0; c < g->cols; c++){
			i = r*g->cols + c;
			a = &g->next[i];
			sid = a->style < t->n ? (int)a->style : 0;
			changed = differs(g, t, i);
			if(!changed && a->span == 2 && c + 1 < g->cols)
				changed = differs(g, t, i + 1);
			if(!changed && a->span == 0 && c > 0)
				changed = differs(g, t, i - 1);
			if(!changed){
				open = 0;
				continue;
			}
			if(a->span == 0){
				/* continuation: part of the lead's run */
				if(!open)
					continue;
				cur->n++;
				ncells++;
				if(cur->pad != 0)
					open = 0;
				continue;
			}
			if(open && cur->style == sid && cur->pad == 0 && a->pad == 0){
				cur->n++;
				cur->nr++;
			}else{
				cur = &out[nrun++];
				cur->row = r;
				cur->col = c;
				cur->n = 1;
				cur->nr = 1;
				cur->pad = a->pad;
				cur->style = sid;
				styleeffective(&t->s[sid], &cur->fg, &cur->bg);
				cur->attr = t->s[sid].attr;
				cur->r = rbuf + nrunes;
				open = 1;
			}
			rbuf[nrunes++] = a->r;
			ncells++;
			if(a->span == 1 && a->pad != 0)
				open = 0;
		}
	}
	*ncell = ncells;
	return nrun;
}

/*
 * shown records what was drawn: an id not yet in t (drawn as default) is
 * stored as 0, so when cellmix or cellsetstyle later creates that id the
 * cells holding it differ from shown and are repainted (WR-01).
 */
void
gridcommit(Grid *g, Styletab *t)
{
	int i, n;

	n = g->rows*g->cols;
	if(n > 0)
		memmove(g->shown, g->next, n * sizeof(Gcell));
	for(i = 0; i < n; i++)
		if(g->shown[i].style >= t->n)
			g->shown[i].style = 0;
	g->force = 0;
}

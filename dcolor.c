/* dcolor.c: cache of 1x1 replicated colour images, one per distinct 0xRRGGBBFF. */
#include "compat.h"
#include <draw.h>
#include "cell.h"
#include "cellcore.h"
#include "celldraw.h"

Image*
colorimage(Cell *c, u32int key)
{
	Image *img;
	int i;

	for(i = 0; i < c->ncol; i++)
		if(c->col[i].key == key)
			return c->col[i].img;
	if(c->ncol >= MaxColor){
		werrstr("too many colours");
		return nil;
	}
	img = allocimage(display, Rect(0,0,1,1), RGB24, 1, key);
	if(img == nil)
		return nil;	/* libdraw has set errstr */
	c->col[c->ncol].key = key;
	c->col[c->ncol].img = img;
	c->col[c->ncol].live = 0;
	c->ncol++;
	return img;
}

void
colorfree(Cell *c)
{
	int i;

	for(i = 0; i < c->ncol; i++)
		freeimage(c->col[i].img);
	c->ncol = 0;
}

/* after a successful flushimage: every cached image now exists on the server */
void
colorlive(Cell *c)
{
	int i;

	for(i = 0; i < c->ncol; i++)
		c->col[i].live = 1;
}

/*
 * Drop the cache after a failed flush.  lost: the buffer holding the
 * unconfirmed images' 'b' messages was discarded (flushimage returned -1),
 * so those images never reached the server: free only their client structs,
 * since an 'f' for an unknown id makes devdraw reject the whole next batch.
 * Otherwise their 'b' is still queued ahead of the 'f' and freeimage is right.
 * The 'f's are then pushed on their own, result ignored, so a bad one can
 * only fail this write and never the next real batch.  An image the server
 * did create before a failing message in the lost batch is leaked.
 * The caller's errstr is preserved.
 */
void
colordrop(Cell *c, int lost)
{
	char err[ERRMAX];
	int i;

	rerrstr(err, sizeof err);	/* keep the caller's error past our flushimage */
	for(i = 0; i < c->ncol; i++)
		if(c->col[i].live || !lost)
			freeimage(c->col[i].img);
		else
			free(c->col[i].img);
	c->ncol = 0;
	flushimage(display, 0);
	werrstr("%s", err);
}

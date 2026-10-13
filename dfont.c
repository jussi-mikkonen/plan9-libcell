/* dfont.c: cell metrics from a caller-supplied font. */
#include "compat.h"
#include <draw.h>
#include "cell.h"
#include "cellcore.h"
#include "celldraw.h"

/* Wtab callback: the width call can load subfonts and print on fd 2, so it
 * runs only on a Wtab miss (once per distinct rune), never per frame. */
static int
fontmeasure(void *arg, Rune r)
{
	return runestringnwidth((Font*)arg, &r, 1);
}

void
fontwtab(Wtab *w, Font *f, int cw)
{
	wtabinit(w, cw, fontmeasure, f);
}

/*
 * Measured once at cellopen: the width call can load subfonts and print on
 * fd 2, so it never runs per frame. Never reads the font cache width field.
 */
int
fontcell(Font *f, int *cw, int *ch)
{
	Rune r;
	char *name, msg[Errlen];
	int w[95];

	name = f->name != nil ? f->name : "(unnamed)";
	for(r = 0x20; r <= 0x7E; r++)
		w[r - 0x20] = runestringnwidth(f, &r, 1);
	if(widthcheck(name, w, 95, cw, msg, sizeof msg) < 0){
		werrstr("%s", msg);
		return -1;
	}
	if(f->height <= 0){
		werrstr("cellopen: %.64s: bad font height", name);
		return -1;
	}
	*ch = (int)f->height;
	return 0;
}

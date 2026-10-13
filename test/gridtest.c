/* gridtest.c: script-driven headless driver for style.c, grid.c and diff.c. */
#include "compat.h"
#include "cellcore.h"
#define HEXHELP
#include "script.h"

static Grid g;
static Styletab st;
static Run *runs;
static Rune *rbuf;
static int havegrid;

/* fake font: every rune is cw px unless overridden */
static struct {
	int cw;
	int eaw;		/* default px is eawcells(r) * cw */
	int n;
	Rune r[32];
	int px[32];
} fake;
static Wtab wt;

static int
fakemeasure(void *arg, Rune r)
{
	int i;

	(void)arg;
	for(i = 0; i < fake.n; i++)
		if(fake.r[i] == r)
			return fake.px[i];
	return fake.eaw ? eawcells(r) * fake.cw : fake.cw;
}

static char*
skipsp(char *p)
{
	while(*p == ' ')
		p++;
	return p;
}

/* space-delimited token; NUL-terminates it and advances *pp; nil if none */
static char*
nexttok(char **pp)
{
	char *p, *t;

	p = skipsp(*pp);
	if(*p == '\0')
		return nil;
	t = p;
	while(*p != ' ' && *p != '\0')
		p++;
	if(*p == ' ')
		*p++ = '\0';
	*pp = p;
	return t;
}

/* text between the next pair of double quotes, raw bytes; nil if malformed */
static char*
nextquoted(char **pp)
{
	char *p, *t;

	p = skipsp(*pp);
	if(*p != '"')
		return nil;
	t = ++p;
	while(*p != '"' && *p != '\0')
		p++;
	if(*p != '"')
		return nil;
	*p++ = '\0';
	*pp = p;
	return t;
}

static int
nextint(char **pp, int *v)
{
	char *p;
	int neg, nd, x;

	p = skipsp(*pp);
	neg = 0;
	if(*p == '-'){
		neg = 1;
		p++;
	}
	x = 0;
	nd = 0;
	while(*p >= '0' && *p <= '9'){
		x = x*10 + (*p - '0');
		nd++;
		p++;
		if(x > 100000000)
			return -1;
	}
	if(nd == 0 || (*p != ' ' && *p != '\0'))
		return -1;
	*v = neg ? -x : x;
	*pp = p;
	return 0;
}

static void
badargs(void)
{
	lprint("error bad args\n");
}

static void
putrunes(Rune *r, int n)
{
	char b[5];
	int i, k;

	for(i = 0; i < n; i++){
		k = utfencode(b, r[i]);
		b[k] = '\0';
		lprint("%s", b);
	}
}

static void
sizecmd(char *a)
{
	int r, c;

	if(nextint(&a, &r) < 0 || nextint(&a, &c) < 0){
		badargs();
		return;
	}
	if(havegrid){
		gridfree(&g);
		free(runs);
		free(rbuf);
		runs = nil;
		rbuf = nil;
		havegrid = 0;
	}
	if(gridinit(&g, r, c) < 0){
		lprint("error grid too large\n");
		return;
	}
	runs = czalloc((g.rows*g.cols > 0 ? g.rows*g.cols : 1) * sizeof(Run));
	rbuf = czalloc((g.rows*g.cols > 0 ? g.rows*g.cols : 1) * sizeof(Rune));
	if(runs == nil || rbuf == nil){
		lprint("error out of memory\n");
		return;
	}
	g.w = &wt;
	havegrid = 1;
}

static void
resizecmd(char *a)
{
	Run *nr;
	Rune *nb;
	int r, c, n;

	if(nextint(&a, &r) < 0 || nextint(&a, &c) < 0 || !havegrid){
		badargs();
		return;
	}
	if(r < 0)
		r = 0;
	if(c < 0)
		c = 0;
	if(c != 0 && r > MaxCells/c){
		lprint("error grid too large\n");
		return;
	}
	n = r*c > 0 ? r*c : 1;
	nr = czalloc(n * sizeof(Run));
	nb = czalloc(n * sizeof(Rune));
	if(nr == nil || nb == nil){
		free(nr);
		free(nb);
		lprint("error out of memory\n");
		return;
	}
	if(gridresize(&g, r, c) < 0){
		free(nr);
		free(nb);
		lprint("error grid too large\n");
		return;
	}
	free(runs);
	free(rbuf);
	runs = nr;
	rbuf = nb;
}

static void
scrollcmd(char *a)
{
	int r, nr, n, src, dst, cnt;

	if(nextint(&a, &r) < 0 || nextint(&a, &nr) < 0 || nextint(&a, &n) < 0 || !havegrid){
		badargs();
		return;
	}
	if(gridscroll(&g, r, nr, n, &src, &dst, &cnt))
		lprint("blit %d %d %d\n", src, dst, cnt);
	else
		lprint("noblit\n");
}

static void
defstylecmd(char *a)
{
	char *name, *fgs, *bgs, *at;
	u32int fg, bg;
	int attr, existed;

	name = nexttok(&a);
	fgs = nexttok(&a);
	bgs = nexttok(&a);
	at = nexttok(&a);
	if(name == nil || fgs == nil || bgs == nil || at == nil
	|| parsecolour(fgs, (int)strlen(fgs), &fg) < 0
	|| parsecolour(bgs, (int)strlen(bgs), &bg) < 0){
		lprint("error bad defstyle\n");
		return;
	}
	attr = 0;
	for(; *at != '\0'; at++){
		if(*at == 'u')
			attr |= Aunder;
		else if(*at == 'r')
			attr |= Arev;
		else if(*at != '-'){
			lprint("error bad defstyle\n");
			return;
		}
	}
	/* the same pure call cellsetstyle makes; cell.c adds colorfree on existed */
	if(stylesetresolve(&st, name, (int)strlen(name), fg, bg, attr, &existed) < 0){
		lprint("error bad defstyle\n");
		return;
	}
	if(existed && havegrid)
		gridinvalidate(&g);
}

static void
printtab(Styletab *t)
{
	int i;

	for(i = 0; i < t->n; i++){
		lprint("%d %s fg=#%06x bg=#%06x ", i,
			t->s[i].name[0] == '\0' ? "*" : t->s[i].name,
			(unsigned)(t->s[i].fg >> 8), (unsigned)(t->s[i].bg >> 8));
		if(t->s[i].attr == 0)
			lprint("-");
		else{
			if(t->s[i].attr & Aunder)
				lprint("u");
			if(t->s[i].attr & Arev)
				lprint("r");
		}
		if(t->s[i].mixa >= 0)
			lprint(" mix=%d,%d", (int)t->s[i].mixa, (int)t->s[i].mixb);
		lprint("\n");
	}
}

/* mirrors cellloadthemestr: success invalidates the grid, failure leaves everything alone */
static void
stylecmd(char *a)
{
	char err[Errlen];

	if(themeparse(&st, a, err, sizeof err) < 0){
		lprint("error %s\n", err);
		return;
	}
	if(havegrid)
		gridinvalidate(&g);
}

/* style token: @N is a raw id, otherwise a name */
static int
styletok(char *s)
{
	int i, v;

	if(s[0] != '@')
		return stylelookup(&st, s);
	v = 0;
	for(i = 1; s[i] >= '0' && s[i] <= '9' && i <= 9; i++)
		v = v*10 + (s[i] - '0');
	if(i == 1 || s[i] != '\0')
		return -2;
	return v;
}

static void
putcmd(char *a)
{
	char *text, *sname;
	int r, c, sid;

	if(nextint(&a, &r) < 0 || nextint(&a, &c) < 0
	|| (text = nextquoted(&a)) == nil || (sname = nexttok(&a)) == nil){
		badargs();
		return;
	}
	if((sid = styletok(sname)) == -2){
		badargs();
		return;
	}
	lprint("put %d\n", gridputs(&g, r, c, text, sid));
}

static void
pointcmd(char *a)
{
	int cw, ch, dx, dy, r, c, in;

	if(nextint(&a, &cw) < 0 || nextint(&a, &ch) < 0
	|| nextint(&a, &dx) < 0 || nextint(&a, &dy) < 0){
		badargs();
		return;
	}
	in = gridpoint(&g, cw, ch, dx, dy, &r, &c);
	lprint("point %d %d %s\n", r, c, in ? "in" : "out");
}

static void
runecmd(char *a)
{
	char *sname;
	long v;
	int r, c, sid;

	if(nextint(&a, &r) < 0 || nextint(&a, &c) < 0
	|| (v = nexthex(&a)) < 0 || (sname = nexttok(&a)) == nil){
		badargs();
		return;
	}
	if((sid = styletok(sname)) == -2){
		badargs();
		return;
	}
	lprint("rune %d\n", gridput(&g, r, c, (Rune)v, sid));
}

static void
fillcmd(char *a)
{
	char *text, *sname;
	Rune ru;
	int r, c, nr, nc, sid;

	if(nextint(&a, &r) < 0 || nextint(&a, &c) < 0
	|| nextint(&a, &nr) < 0 || nextint(&a, &nc) < 0
	|| (text = nextquoted(&a)) == nil || (sname = nexttok(&a)) == nil){
		badargs();
		return;
	}
	if((sid = styletok(sname)) == -2){
		badargs();
		return;
	}
	utfdecode(text, &ru);
	gridfill(&g, r, c, nr, nc, ru, sid);
}

static void
cwcmd(char *a)
{
	int v;

	if(nextint(&a, &v) < 0 || v < 1){
		badargs();
		return;
	}
	fake.cw = v;
	wtabinit(&wt, fake.cw, fakemeasure, &fake);
}

static void
widthcmd(char *a)
{
	char *w;
	long v;
	int px, i;

	w = skipsp(a);
	if(strcmp(w, "eaw") == 0 || strcmp(w, "cw") == 0){
		fake.eaw = w[0] == 'e';
		fake.n = 0;
		wtabinit(&wt, fake.cw, fakemeasure, &fake);
		return;
	}
	if((v = nexthex(&a)) < 0 || nextint(&a, &px) < 0){
		badargs();
		return;
	}
	for(i = 0; i < fake.n; i++)
		if(fake.r[i] == (Rune)v)
			break;
	if(i == fake.n){
		if(fake.n >= 32){
			lprint("error too many widths\n");
			return;
		}
		fake.n++;
	}
	fake.r[i] = (Rune)v;
	fake.px[i] = px;
	wtabinit(&wt, fake.cw, fakemeasure, &fake);
}

static void
spancmd(char *a)
{
	Rune out;
	long v;
	int cells, pad;

	if((v = nexthex(&a)) < 0){
		badargs();
		return;
	}
	cells = runespan(&wt, (Rune)v, &out, &pad);
	lprint("span %04x -> %04x %d pad=%d\n", (unsigned)v, (unsigned)out, cells, pad);
}

static void
eawcmd(char *a)
{
	long v;

	if((v = nexthex(&a)) < 0){
		badargs();
		return;
	}
	lprint("eaw %04x %d\n", (unsigned)v, eawcells((Rune)v));
}

static void
dumpcmd(void)
{
	int r, c;

	for(r = 0; r < g.rows; r++){
		lprint("%d |", r);
		for(c = 0; c < g.cols; c++)
			if(g.next[r*g.cols + c].span != 0)
				putrunes(&g.next[r*g.cols + c].r, 1);
		lprint("|\n");
	}
}

static void
cellscmd(char *a)
{
	Gcell *x;
	int r, c;

	if(nextint(&a, &r) < 0){
		badargs();
		return;
	}
	if(r < 0 || r >= g.rows){
		lprint("error bad row\n");
		return;
	}
	for(c = 0; c < g.cols; c++){
		x = &g.next[r*g.cols + c];
		lprint("%d %04x %d %d %d\n", c, (unsigned)x->r, x->span, x->style, x->pad);
	}
}

static void
flushcmd(int fail)
{
	int i, n, nrun;

	nrun = griddiff(&g, &st, runs, rbuf, &n);
	if(fail){
		lprint("failflush %d\n", n);
		gridinvalidate(&g);
		return;
	}
	lprint("flush %d\n", n);
	for(i = 0; i < nrun; i++){
		lprint("%d %d %d #%06x #%06x ", runs[i].row, runs[i].col, runs[i].n,
			(unsigned)(runs[i].fg >> 8), (unsigned)(runs[i].bg >> 8));
		if(runs[i].attr == 0)
			lprint("-");
		else{
			if(runs[i].attr & Aunder)
				lprint("u");
			if(runs[i].attr & Arev)
				lprint("r");
		}
		lprint(" \"");
		putrunes(runs[i].r, runs[i].nr);
		lprint("\"");
		if(runs[i].pad != 0)
			lprint(" pad=%d", runs[i].pad);
		lprint("\n");
	}
	gridcommit(&g, &st);
}

/* mirrors cellopen's load; success invalidates an existing grid */
static void
builtincmd(void)
{
	char err[Errlen];

	if(themebuiltin(&st, err, sizeof err) < 0){
		lprint("error %s\n", err);
		return;
	}
	if(havegrid)
		gridinvalidate(&g);
}

static void
mixcmd(char *a)
{
	char err[Errlen];
	int x, y, id;

	if(nextint(&a, &x) < 0 || nextint(&a, &y) < 0){
		badargs();
		return;
	}
	id = stylemix(&st, x, y, err, sizeof err);
	lprint("mix %d %d -> %d\n", x, y, id);
	if(id < 0)
		lprint("error %s\n", err);
}

static void
lookupcmd(char *a)
{
	char *name;

	if((name = nexttok(&a)) == nil){
		badargs();
		return;
	}
	lprint("lookup %s %d\n", name, stylelookup(&st, name));
}

static void
xcmd(char *l)
{
	char *cmd;

	cmd = nexttok(&l);
	if(cmd == nil)
		return;
	if(strcmp(cmd, "size") == 0){
		sizecmd(l);
		return;
	}
	if(strcmp(cmd, "resize") == 0){
		resizecmd(l);
		return;
	}
	if(strcmp(cmd, "scroll") == 0){
		scrollcmd(l);
		return;
	}
	if(strcmp(cmd, "defstyle") == 0){
		defstylecmd(l);
		return;
	}
	if(strcmp(cmd, "lookup") == 0){
		lookupcmd(l);
		return;
	}
	if(strcmp(cmd, "style") == 0){
		stylecmd(l);
		return;
	}
	if(strcmp(cmd, "builtin") == 0){
		builtincmd();
		return;
	}
	if(strcmp(cmd, "mix") == 0){
		mixcmd(l);
		return;
	}
	if(strcmp(cmd, "styles") == 0){
		printtab(&st);
		return;
	}
	if(strcmp(cmd, "cw") == 0){
		cwcmd(l);
		return;
	}
	if(strcmp(cmd, "width") == 0){
		widthcmd(l);
		return;
	}
	if(strcmp(cmd, "measures") == 0){
		lprint("measures %d\n", wt.nmeasure);
		return;
	}
	if(strcmp(cmd, "eaw") == 0){
		eawcmd(l);
		return;
	}
	if(strcmp(cmd, "eawcheck") == 0){
		if(eawcheck() < 0)
			lprint("eawcheck bad\n");
		else
			lprint("eawcheck ok %d\n", eawcheck());
		return;
	}
	if(strcmp(cmd, "span") == 0){
		spancmd(l);
		return;
	}
	if(!havegrid){
		lprint("error no grid\n");
		return;
	}
	if(strcmp(cmd, "put") == 0)
		putcmd(l);
	else if(strcmp(cmd, "point") == 0)
		pointcmd(l);
	else if(strcmp(cmd, "rune") == 0)
		runecmd(l);
	else if(strcmp(cmd, "fill") == 0)
		fillcmd(l);
	else if(strcmp(cmd, "clear") == 0)
		gridclear(&g);
	else if(strcmp(cmd, "invalidate") == 0)
		gridinvalidate(&g);
	else if(strcmp(cmd, "cells") == 0)
		cellscmd(l);
	else if(strcmp(cmd, "dump") == 0)
		dumpcmd();
	else if(strcmp(cmd, "flush") == 0)
		flushcmd(0);
	else if(strcmp(cmd, "failflush") == 0)
		flushcmd(1);
	else
		lprint("error unknown command\n");
}

static int
run(void)
{
	styleinit(&st);
	fake.cw = 8;
	wtabinit(&wt, 8, fakemeasure, &fake);
	if(script(LINEMAX, xcmd))
		return 1;
	if(havegrid){
		gridfree(&g);
		free(runs);
		free(rbuf);
	}
	return 0;
}

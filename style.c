/* style.c: pure style table and colour parsing. */
#include "compat.h"
#include "cellcore.h"

void
styleinit(Styletab *t)
{
	memset(t, 0, sizeof *t);
	memmove(t->s[0].name, "default", 8);
	t->s[0].fg = 0xd0d0d0ff;
	t->s[0].bg = 0x1c1c1cff;
	t->s[0].attr = 0;
	t->s[0].sfg = t->s[0].fg;
	t->s[0].sbg = t->s[0].bg;
	t->s[0].has = 3;
	t->s[0].mixa = t->s[0].mixb = -1;
	t->n = 1;
}

/* style 0 is the source of truth; others follow it for colours they did not set */
void
styleresolve(Styletab *t)
{
	int i;

	t->s[0].sfg = t->s[0].fg;
	t->s[0].sbg = t->s[0].bg;
	t->s[0].has = 3;
	/* mix sources always have lower ids, so ascending id order is a valid topological order */
	for(i = 1; i < t->n; i++){
		if(t->s[i].mixa >= 0){
			Style *m = &t->s[i], *a = &t->s[m->mixa], *b = &t->s[m->mixb];

			m->has = a->has | b->has;
			m->sfg = (b->has & 1) ? b->sfg : a->sfg;
			m->sbg = (b->has & 2) ? b->sbg : a->sbg;
			m->attr = a->attr | b->attr;
		}
		t->s[i].fg = (t->s[i].has & 1) ? t->s[i].sfg : t->s[0].fg;
		t->s[i].bg = (t->s[i].has & 2) ? t->s[i].sbg : t->s[0].bg;
	}
}

void
styleeffective(Style *s, u32int *fg, u32int *bg)
{
	if(s->attr & Arev){
		*fg = s->bg;
		*bg = s->fg;
	}else{
		*fg = s->fg;
		*bg = s->bg;
	}
}

static int
namechar(int c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
		|| (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-';
}

static int
namevalid(char *name, int nname)
{
	int i;

	if(name == nil || nname < 1 || nname > MaxName)
		return 0;
	for(i = 0; i < nname; i++)
		if(!namechar((uchar)name[i]))
			return 0;
	return 1;
}

int
stylefind(Styletab *t, char *name, int nname)
{
	int i;

	if(name == nil || nname < 1)
		return -1;
	for(i = 0; i < t->n; i++)
		if((int)strlen(t->s[i].name) == nname && memcmp(t->s[i].name, name, nname) == 0)
			return i;
	return -1;
}

int
styleset(Styletab *t, char *name, int nname, u32int fg, u32int bg, int attr)
{
	int i;

	if(!namevalid(name, nname))
		return -1;
	i = stylefind(t, name, nname);
	if(i < 0){
		if(t->n >= MaxStyle)
			return -1;
		i = t->n++;
		memmove(t->s[i].name, name, nname);
		t->s[i].name[nname] = '\0';
	}
	t->s[i].fg = fg;
	t->s[i].bg = bg;
	t->s[i].attr = attr;
	t->s[i].sfg = fg;
	t->s[i].sbg = bg;
	t->s[i].has = 3;
	t->s[i].mixa = t->s[i].mixb = -1;
	return i;
}

/*
 * b's set fields over a's.  Default (0) as the overlay adds nothing: the
 * result is a, not a copy of default that would wipe a's colours; mixing
 * a style with itself is also a.  Bad ids fail instead of becoming default.
 */
/*
 * cellsetstyle's pure part: define or redefine name, then resolve.
 * *existed is set when name was already defined: its cells keep the same
 * (rune, id), so the caller must drop cached colours and invalidate the grid.
 */
int
stylesetresolve(Styletab *t, char *name, int nname, u32int fg, u32int bg, int attr, int *existed)
{
	int id;

	*existed = stylefind(t, name, nname) >= 0;
	id = styleset(t, name, nname, fg, bg, attr);
	if(id < 0)
		return -1;
	styleresolve(t);
	return id;
}

int
stylemix(Styletab *t, int a, int b, char *err, int nerr)
{
	int i;

	if(a < 0 || a >= t->n || b < 0 || b >= t->n){
		if(err != nil && nerr > 0)
			lsnprint(err, nerr, "bad style id");
		return -1;
	}
	if(b == 0 || a == b)
		return a;
	for(i = 1; i < t->n; i++)
		if(t->s[i].mixa == a && t->s[i].mixb == b)
			return i;
	if(t->n >= MaxStyle){
		if(err != nil && nerr > 0)
			lsnprint(err, nerr, "too many styles");
		return -1;
	}
	i = t->n++;
	t->s[i].name[0] = '\0';
	t->s[i].mixa = a;
	t->s[i].mixb = b;
	styleresolve(t);
	return i;
}

int
stylelookup(Styletab *t, char *name)
{
	int i;

	if(name == nil)
		return 0;
	i = stylefind(t, name, (int)strlen(name));
	return i < 0 ? 0 : i;
}

int
parsecolour(char *s, int n, u32int *c)
{
	u32int v;
	int i, ch, d;

	if(s == nil || n != 7 || s[0] != '#')
		return -1;
	v = 0;
	for(i = 1; i < 7; i++){
		ch = (uchar)s[i];
		if(ch >= '0' && ch <= '9')
			d = ch - '0';
		else if(ch >= 'a' && ch <= 'f')
			d = ch - 'a' + 10;
		else if(ch >= 'A' && ch <= 'F')
			d = ch - 'A' + 10;
		else
			return -1;
		v = (v << 4) | (u32int)d;
	}
	*c = (v << 8) | 0xFF;
	return 0;
}

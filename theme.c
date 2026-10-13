/*
 * Styles keep the colours as written plus set flags; every load ends with styleresolve, so
 * unset colours follow the current default. A later line replaces an earlier one wholesale.
 * The built-in theme text lives here (themebuiltin), loaded by cellopen.
 *
 * theme.c: pure theme parser. Never modifies its text input, never touches
 * *tab unless the whole text parsed. Missing fg/bg of every style defined in the text
 * come from the resolved "default" line, wherever that line is. Attributes are exactly
 * those written on the style's line.
 */
#include "compat.h"
#include "cellcore.h"

typedef struct Rec Rec;
struct Rec {
	int used;		/* defined by a line of this load */
	int hasfg, hasbg;
	u32int fg, bg;
	int attr;
};

static int
isname(int c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
		|| (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-';
}

/* "theme:LINE: msg" or "theme:LINE: msg 'tok'" (tok copied and cut to 32 bytes) */
static void
fail(char *err, int nerr, int lineno, char *msg, char *tok, int ntok)
{
	char t[33];

	if(err == nil || nerr <= 0)
		return;
	if(tok == nil){
		lsnprint(err, nerr, "theme:%d: %s", lineno, msg);
		return;
	}
	if(ntok > 32)
		ntok = 32;
	memmove(t, tok, ntok);
	t[ntok] = '\0';
	lsnprint(err, nerr, "theme:%d: %s '%.32s'", lineno, msg, t);
}

/* tokens of one line [p, e): returns next token start or nil; sets *n */
static char*
nexttoken(char **pp, char *e, int *n)
{
	char *p, *t;

	p = *pp;
	while(p < e && (*p == ' ' || *p == '\t'))
		p++;
	if(p >= e){
		*pp = p;
		return nil;
	}
	t = p;
	while(p < e && *p != ' ' && *p != '\t')
		p++;
	*n = p - t;
	*pp = p;
	return t;
}

/* parse one line into rec/tmp; returns 0, or -1 with err filled */
static int
parseline(Styletab *tmp, Rec *rec, char *p, char *e, int lineno, char *err, int nerr)
{
	Rec r;
	char *tok, *eq;
	int n, i, id, seenfg, seenbg, seenu, seenr, klen;

	tok = nexttoken(&p, e, &n);
	if(tok == nil || tok[0] == '#')
		return 0;
	if(n > MaxName){
		fail(err, nerr, lineno, "name too long", nil, 0);
		return -1;
	}
	for(i = 0; i < n; i++)
		if(!isname((uchar)tok[i])){
			fail(err, nerr, lineno, "bad style name", tok, n);
			return -1;
		}
	id = stylefind(tmp, tok, n);
	if(id < 0 && tmp->n >= MaxStyle){
		fail(err, nerr, lineno, "too many styles", nil, 0);
		return -1;
	}
	/* the name is remembered by position: it is the first token of the line */
	memset(&r, 0, sizeof r);
	r.used = 1;
	seenfg = seenbg = seenu = seenr = 0;
	{
		char *name = tok;
		int nname = n;

		for(;;){
			tok = nexttoken(&p, e, &n);
			if(tok == nil || tok[0] == '#')
				break;
			eq = nil;
			for(i = 0; i < n; i++)
				if(tok[i] == '='){
					eq = tok + i;
					break;
				}
			if(eq == nil){
				if(n == 9 && memcmp(tok, "underline", 9) == 0){
					if(seenu){
						fail(err, nerr, lineno, "duplicate key", tok, n);
						return -1;
					}
					seenu = 1;
					r.attr |= Aunder;
				}else if(n == 7 && memcmp(tok, "reverse", 7) == 0){
					if(seenr){
						fail(err, nerr, lineno, "duplicate key", tok, n);
						return -1;
					}
					seenr = 1;
					r.attr |= Arev;
				}else{
					fail(err, nerr, lineno, "unknown flag", tok, n);
					return -1;
				}
				continue;
			}
			klen = eq - tok;
			if(klen == 2 && (memcmp(tok, "fg", 2) == 0 || memcmp(tok, "bg", 2) == 0)){
				u32int c;
				int isfg = tok[0] == 'f';

				if((isfg && seenfg) || (!isfg && seenbg)){
					fail(err, nerr, lineno, "duplicate key", tok, klen);
					return -1;
				}
				if(n - klen - 1 == 0){
					fail(err, nerr, lineno, "empty value", tok, klen);
					return -1;
				}
				if(parsecolour(eq + 1, n - klen - 1, &c) < 0){
					fail(err, nerr, lineno, "bad colour", eq + 1, n - klen - 1);
					return -1;
				}
				if(isfg){
					seenfg = 1;
					r.hasfg = 1;
					r.fg = c;
				}else{
					seenbg = 1;
					r.hasbg = 1;
					r.bg = c;
				}
				continue;
			}
			fail(err, nerr, lineno, "unknown key", tok, klen);
			return -1;
		}
		/* existing names keep their id and, until pass 2, their colours */
		if(id < 0)
			id = styleset(tmp, name, nname, 0, 0, 0);
		if(id < 0){
			fail(err, nerr, lineno, "bad style name", name, nname);
			return -1;
		}
	}
	rec[id] = r;	/* last line wins, wholesale */
	return 0;
}

int
themeparse(Styletab *tab, char *text, char *err, int nerr)
{
	Styletab *tmp;
	Rec *rec;
	char *p, *e, *nl;
	int lineno, i, rc;

	if(err != nil && nerr > 0)
		err[0] = '\0';
	if(text == nil){
		fail(err, nerr, 0, "nil text", nil, 0);
		return -1;
	}
	tmp = czalloc(sizeof *tmp);
	rec = czalloc(MaxStyle * sizeof *rec);
	if(tmp == nil || rec == nil){
		free(tmp);
		free(rec);
		if(err != nil && nerr > 0)
			lsnprint(err, nerr, "theme: out of memory");
		return -1;
	}
	memmove(tmp, tab, sizeof *tmp);

	rc = 0;
	lineno = 0;
	p = text;
	while(*p != '\0'){
		nl = p;
		while(*nl != '\0' && *nl != '\n')
			nl++;
		e = nl;
		if(e > p && e[-1] == '\r')
			e--;
		lineno++;
		if(parseline(tmp, rec, p, e, lineno, err, nerr) < 0){
			rc = -1;
			break;
		}
		p = nl;
		if(*p == '\n')
			p++;
	}
	if(rc == 0){
		/* pass 2: default first, then everything else from the resolved default */
		if(rec[0].used){
			if(rec[0].hasfg)
				tmp->s[0].fg = rec[0].fg;
			if(rec[0].hasbg)
				tmp->s[0].bg = rec[0].bg;
			tmp->s[0].attr = rec[0].attr;
		}
		for(i = 1; i < tmp->n; i++){
			if(!rec[i].used)
				continue;
			tmp->s[i].sfg = rec[i].fg;
			tmp->s[i].sbg = rec[i].bg;
			tmp->s[i].has = (rec[i].hasfg ? 1 : 0) | (rec[i].hasbg ? 2 : 0);
			tmp->s[i].attr = rec[i].attr;
			tmp->s[i].mixa = tmp->s[i].mixb = -1;
		}
		styleresolve(tmp);
		memmove(tab, tmp, sizeof *tab);
	}
	free(tmp);
	free(rec);
	return rc;
}

static char builtin[] =
	"default   fg=#d0d0d0 bg=#1c1c1c\n"
	"keyword   fg=#5f87af\n"
	"comment   fg=#808080\n"
	"string    fg=#87af5f\n"
	"error     fg=#ff5f5f underline\n"
	"selection bg=#444444\n"
	"cursor    reverse\n";

int
themebuiltin(Styletab *tab, char *err, int nerr)
{
	return themeparse(tab, builtin, err, nerr);
}

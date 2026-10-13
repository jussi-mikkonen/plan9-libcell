/* themetest.c: stdin is theme text; prints the error line (if any), then the style table. */
#include "compat.h"
#include "cellcore.h"
#define NOSCRIPT
#include "script.h"

static void
printtab(Styletab *t)
{
	int i;

	for(i = 0; i < t->n; i++){
		lprint("%d %s fg=#%06x bg=#%06x ", i, t->s[i].name,
			(unsigned)(t->s[i].fg >> 8), (unsigned)(t->s[i].bg >> 8));
		if(t->s[i].attr == 0)
			lprint("-");
		else{
			if(t->s[i].attr & Aunder)
				lprint("u");
			if(t->s[i].attr & Arev)
				lprint("r");
		}
		lprint("\n");
	}
}

/* run one chunk: "@builtin" loads the built-in theme, anything else is theme text */
static void
loadchunk(Styletab *st, char *text, int n)
{
	static char chunk[INMAX+1];
	char err[Errlen];
	int m, rc;

	memmove(chunk, text, n);
	chunk[n] = '\0';
	m = n;
	if(m > 0 && chunk[m-1] == '\n')
		m--;
	if(m > 0 && chunk[m-1] == '\r')
		m--;
	if(m == 8 && memcmp(chunk, "@builtin", 8) == 0)
		rc = themebuiltin(st, err, sizeof err);
	else
		rc = themeparse(st, chunk, err, sizeof err);
	if(rc < 0)
		lprint("error %s\n", err);
	printtab(st);
}

static int
run(void)
{
	static Styletab st;
	char *p, *nl, *e, *start;
	int first;

	if(readall())
		return 1;
	styleinit(&st);
	first = 1;
	start = p = inbuf;
	while(*p != '\0'){
		nl = p;
		while(*nl != '\0' && *nl != '\n')
			nl++;
		e = nl;
		if(e > p && e[-1] == '\r')
			e--;
		if(e - p == 2 && p[0] == '@' && p[1] == '@'){
			if(!first)
				lprint("@@\n");
			first = 0;
			loadchunk(&st, start, p - start);
			start = nl;
			if(*start == '\n')
				start++;
		}
		p = nl;
		if(*p == '\n')
			p++;
	}
	if(!first)
		lprint("@@\n");
	loadchunk(&st, start, p - start);
	return 0;
}

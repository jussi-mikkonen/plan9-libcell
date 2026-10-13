/* widthtest.c: script-driven headless driver for width.c. */
#include "compat.h"
#include "cellcore.h"
#define HEXHELP
#include "script.h"

static int w[95];

static void
xcmd(char *l)
{
	char msg[Errlen], *p;
	long code;
	int i, v, cw;

	if(strncmp(l, "ascii ", 6) == 0){
		v = atoi(l + 6);
		for(i = 0; i < 95; i++)
			w[i] = v;
	}else if(strncmp(l, "set ", 4) == 0){
		p = l + 4;
		code = nexthex(&p);
		if(code < 0x20 || code > 0x7E){
			lprint("error bad rune\n");
			return;
		}
		w[code - 0x20] = atoi(p);
	}else if(strncmp(l, "check ", 6) == 0){
		if(widthcheck(l + 6, w, 95, &cw, msg, sizeof msg) < 0)
			lprint("%s\n", msg);
		else
			lprint("ok cw=%d\n", cw);
	}else
		lprint("error unknown command\n");
}

static int
run(void)
{
	return script(LINEMAX, xcmd);
}

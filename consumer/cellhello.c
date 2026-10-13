#include <u.h>
#include <libc.h>
#include <draw.h>
#include <cell.h>

void
main(void)
{
	Cell *c;
	int n;

	if(initdraw(nil, nil, "cellhello") < 0)
		sysfatal("initdraw: %r");
	c = cellopen(display->defaultfont);
	if(c == nil)
		sysfatal("cellopen: %r");
	cellputs(c, 0, 0, "hello, libcell 漢字", cellstyle(c, "keyword"));
	n = cellflush(c);
	print("cellhello: flush %d cells\n", n);
	sleep(2000);
	cellclose(c);
	exits(nil);
}

/*
 * celldemo.c: libcell demo: fills the window with every style, flushes, types runes.
 * Flags: -n built-in theme only, -b runs benchmark then exit, -f font, -t theme.
 * Keys: typed runes, Enter flush, Ctrl-L invalidate, Ctrl-F full/damage toggle,
 * Up/Down scroll, Del/Esc quit.
 */
#include <u.h>
#include <libc.h>
#include <draw.h>
#include <event.h>
#include <keyboard.h>
#include <cell.h>

Cell *cell;
Font *font;
char *fontname = "/lib/font/bit/vga/unicode.font";
char *themepath = "theme";
enum { Nsty = 10 };
char *labels[Nsty] = { "default", "keyword", "comment", "string", "error", "selection", "cursor",
	"runtime", "mix(sel,kw)", "mix(kw,sel)" };
int ids[Nsty];
int crow, ccol;
int top;	/* logical first line; Up/Down scroll it */
int nouser;	/* -n: built-in theme only */
int fullmode;	/* Ctrl-F: invalidate before every flush */
int benchruns;	/* -b N */

void
usage(void)
{
	fprint(2, "usage: celldemo [-n] [-b runs] [-f font] [-t theme]\n");
	exits("usage");
}

void
flush(void)
{
	int n;

	if(fullmode)
		cellinvalidate(cell);
	n = cellflush(cell);
	if(n < 0)
		fprint(2, "celldemo: cellflush: %r\n");
	else
		print("flush: %d cells\n", n);
}

void
putline(int r)
{
	char buf[128];
	int l, rows, cols, id;

	cellsize(cell, &rows, &cols);
	l = top + r;
	id = ids[l % Nsty];
	cellfill(cell, r, 0, 1, cols, ' ', id);
	if(l % 4 == 3){
		/* wide row: CJK, Hangul, fullwidth, box drawing, accent */
		snprint(buf, sizeof buf, "%-11s row %02d: ", labels[l % Nsty], l);
		cellputs(cell, r, 0, buf, id);
		cellputs(cell, r, 20, "漢字 かな 한국어 ＡＢＣ ─│┼ é", id);
		/* lands inside a wide pair: its partner becomes a space */
		cellput(cell, r, 21, 'x', id);
		/* last column: a wide rune does not fit, becomes a space */
		cellputs(cell, r, cols-1, "漢", id);
		return;
	}
	snprint(buf, sizeof buf, "%-11s row %02d: the quick brown fox jumps over the lazy dog", labels[l % Nsty], l);
	cellputs(cell, r, 0, buf, id);
}

void
fill(void)
{
	int r, rows, cols;

	cellsize(cell, &rows, &cols);
	for(r = 0; r < rows; r++)
		putline(r);
	crow = 0;
	ccol = 0;
}

void
opencell(void)
{
	int i, sel, kw;

	cell = cellopen(font);
	if(cell == nil)
		sysfatal("%r");
	if(!nouser && cellloadtheme(cell, themepath) < 0)
		sysfatal("cellloadtheme: %r");
	/* runtime styles live in the Cell, so they are registered on every (re)open */
	for(i = 0; i < 7; i++)
		ids[i] = cellstyle(cell, labels[i]);
	ids[7] = cellsetstyle(cell, "demo", 0xff8800, 0x202040, Cunderline|Creverse);
	if(ids[7] < 0)
		sysfatal("cellsetstyle: %r");
	sel = cellstyle(cell, "selection");
	kw = cellstyle(cell, "keyword");
	ids[8] = cellmix(cell, sel, kw);
	if(ids[8] < 0)
		sysfatal("cellmix: %r");
	ids[9] = cellmix(cell, kw, sel);
	if(ids[9] < 0)
		sysfatal("cellmix: %r");
	if(cellmix(cell, sel, kw) != ids[8])
		sysfatal("cellmix: not cached");
}

/* records the advances of non-vga families on the VM */
void
probe(void)
{
	Rune r[6];
	int cw, ch, i, px[6];

	r[0] = 0x3042; r[1] = 0x4e00; r[2] = 0xac00; r[3] = 0xff21; r[4] = 0x2500; r[5] = 0xfffd;
	for(i = 0; i < 6; i++)
		px[i] = runestringnwidth(font, &r[i], 1);
	cellpixsize(cell, &cw, &ch);
	fprint(2, "celldemo: cw %d ch %d px 3042=%d 4e00=%d ac00=%d ff21=%d 2500=%d fffd=%d\n",
		cw, ch, px[0], px[1], px[2], px[3], px[4], px[5]);
}

/* time full and damage-only flushes */
void
bench(int runs)
{
	vlong t0, tfull, tdam;
	int i, rows, cols;

	cellsize(cell, &rows, &cols);
	if(rows == 0 || cols == 0)
		sysfatal("bench: empty grid");
	t0 = nsec();
	for(i = 0; i < runs; i++){
		cellinvalidate(cell);
		if(cellflush(cell) < 0)
			sysfatal("cellflush: %r");
	}
	tfull = nsec() - t0;
	t0 = nsec();
	for(i = 0; i < runs; i++){
		cellput(cell, i % rows, i % cols, 'a' + i % 26, ids[i % Nsty]);
		if(cellflush(cell) < 0)
			sysfatal("cellflush: %r");
	}
	tdam = nsec() - t0;
	print("bench: %d rows %d cols %d runs full %lld us/flush damage %lld us/flush\n",
		rows, cols, runs, tfull/runs/1000, tdam/runs/1000);
}

void
eresized(int new)
{
	int rows, cols;

	if(new && getwindow(display, Refnone) < 0){
		fprint(2, "celldemo: getwindow: %r\n");
		return;
	}
	if(cellresize(cell) < 0)
		fprint(2, "celldemo: cellresize: %r\n");
	else{
		cellsize(cell, &rows, &cols);
		print("resize: %d rows %d cols\n", rows, cols);
	}
	fill();
	flush();
}

void
main(int argc, char **argv)
{
	Event ev;
	ulong k;
	int rows, cols;

	ARGBEGIN{
	case 'n':
		nouser = 1;
		break;
	case 'b':
		benchruns = atoi(EARGF(usage()));
		if(benchruns < 1)
			usage();
		break;
	case 'f':
		fontname = EARGF(usage());
		break;
	case 't':
		themepath = EARGF(usage());
		break;
	default:
		usage();
	}ARGEND
	if(argc != 0)
		usage();

	if(initdraw(nil, nil, "celldemo") < 0)
		sysfatal("initdraw: %r");
	font = openfont(display, fontname);
	if(font == nil)
		sysfatal("openfont %s: %r", fontname);
	einit(Emouse|Ekeyboard);
	opencell();
	probe();
	fill();
	flush();
	if(benchruns > 0){
		bench(benchruns);
		exits(nil);
	}
	for(;;){
		k = event(&ev);
		if(k != Ekeyboard)
			continue;
		if(ev.kbdc == 0x7F || ev.kbdc == 0x1B)
			break;
		if(ev.kbdc == '\n'){
			flush();
			continue;
		}
		if(ev.kbdc == 0x0C){
			cellinvalidate(cell);
			flush();
			continue;
		}
		if(ev.kbdc == 0x06){
			fullmode = !fullmode;
			print("mode: %s\n", fullmode ? "full" : "damage");
			continue;
		}
		cellsize(cell, &rows, &cols);
		if(ev.kbdc == Kdown || ev.kbdc == Kup){
			if(rows == 0 || cols == 0 || (ev.kbdc == Kup && top == 0))
				continue;
			if(ev.kbdc == Kdown){
				top++;
				cellscroll(cell, 0, rows, 1);
				putline(rows-1);
			}else{
				top--;
				cellscroll(cell, 0, rows, -1);
				putline(0);
			}
			flush();
			continue;
		}
		if(ev.kbdc < 0x20)
			continue;
		if(rows == 0 || cols == 0)
			continue;
		cellput(cell, crow, ccol, ev.kbdc, cellstyle(cell, "string"));
		if(++ccol >= cols){
			ccol = 0;
			if(++crow >= rows)
				crow = 0;
		}
		flush();
	}
	exits(nil);
}

# libcell

A cell-grid colour text library for 9front, built on libdraw.

libcell plays the role ncurses plays elsewhere: styled, coloured text in a
rows x cols grid, where a flush redraws only the cells that changed. It draws
into an ordinary libdraw window, with no terminal and no escape sequences, so
programs using it run unchanged in rio, in rini and nested.

- Styles: foreground and background colour, underline, reverse. Colour images
  are allocated once and cached; callers never call allocimage.
- Themes: programs draw by style name (keyword, comment, error, ...); a small
  text file picks the colours.
- Fixed-width fonts only, with double-width cells for East Asian wide runes.
  A proportional font is an error.
- Resize and scroll; the caller owns the event loop.

## Build and install

On 9front:

	mk install	# /$objtype/lib/libcell.a, /sys/include/cell.h, /sys/man/2/cell
	mk test
	mk demo		# demo/celldemo, a styled-text demo to try in a window

See `man 2 cell` for the API and the theme file format, and `demo/theme` for
an example theme.

## Development

The grid, diff, theme and width code is plain C with no Plan 9 dependencies.
`mk test` runs its golden tests on 9front; on Linux:

	make test

`make check9` and `make lint` check the Plan 9 sources with gcc against a copy
of the 9front tree in `.front-ref/` (skipped when it is absent).
`tools/vmcheck.rc` builds, tests, installs and lints on a 9front machine and
logs the output; `tools/gen-eaw.py` regenerates `eaw.c` from Unicode's
`EastAsianWidth.txt`.

## Notes

Written with the help of an AI coding assistant.

## License

MIT, see [LICENSE](LICENSE).

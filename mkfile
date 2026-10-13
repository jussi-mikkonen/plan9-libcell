# libcell is built with mksyslib straight into /$objtype/lib, like libdraw.
# `mk install` also copies the header and the man page, which mksyslib does not.
</$objtype/mkfile

LIB=/$objtype/lib/libcell.a
OFILES=\
	style.$O\
	grid.$O\
	wtab.$O\
	diff.$O\
	theme.$O\
	width.$O\
	cell.$O\
	dfont.$O\
	dcolor.$O\
	dflush.$O\
	dtheme.$O\

HFILES=cell.h cellcore.h celldraw.h compat.h
CFLAGS=$CFLAGS -DPLAN9 -I.

</sys/src/cmd/mksyslib

install:V: /sys/include/cell.h /sys/man/2/cell

/sys/include/cell.h: cell.h
	cp cell.h $target

/sys/man/2/cell: cell.2
	cp cell.2 $target

# every global symbol defined by a member must start with cell or Cell
lint:V: $LIB
	bad=`{nm $LIB | awk '$2 ~ /^[TDB]$/ && $3 !~ /^(cell|Cell)/ {print $3}'}
	if(! ~ $#bad 0){
		echo lint: symbols without cell prefix: $bad
		exit lint
	}
	echo lint: ok

demo:V: install
	cd demo
	mk

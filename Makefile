CC=gcc
# the host may lack libasan: probe, and say so when sanitizers are off
SAN:=$(shell echo 'int main(void){return 0;}' | $(CC) -x c - -fsanitize=address,undefined -o /dev/null 2>/dev/null && echo -fsanitize=address,undefined -fno-omit-frame-pointer)
CFLAGS=-std=c99 -Wall -Wextra -Werror -g $(SAN) -I.

PURE=style grid diff theme width wtab eaw
IMPURE=cell dfont dcolor dflush dtheme
HDRS=compat.h cellcore.h test/script.h

all: test

test/gridtest: test/gridtest.c $(PURE:%=%.c) $(HDRS)
	$(CC) $(CFLAGS) -o $@ $(filter %.c,$^)

test/themetest: test/themetest.c $(PURE:%=%.c) $(HDRS)
	$(CC) $(CFLAGS) -o $@ $(filter %.c,$^)

test/widthtest: test/widthtest.c width.c $(HDRS)
	$(CC) $(CFLAGS) -o $@ $(filter %.c,$^)

test: test/gridtest test/themetest test/widthtest
	@if [ -n "$(SAN)" ]; then echo "sanitizers: on"; else echo "sanitizers: off (no libasan)"; fi
	test/run-tests.sh
	! grep -nE '#include[[:space:]]*<(draw|thread|9p|mouse|keyboard|frame|event|bio)\.h>' $(PURE:%=%.c) cellcore.h
	! grep -nE '#include[[:space:]]*<' $(PURE:%=%.c) cellcore.h

# gcc -fsyntax-only of the Plan 9 sources against the real 9front headers (.front-ref)
check9:
	@if [ ! -f .front-ref/sys/include/libc.h ]; then echo "check9: skipped (no .front-ref)"; exit 0; fi; \
	rc=0; \
	for f in $(PURE:%=%.c) $(IMPURE:%=%.c) demo/celldemo.c consumer/cellhello.c; do \
		if [ ! -f $$f ]; then echo "check9: $$f does not exist yet, skipped"; continue; fi; \
		echo "check9: $$f"; \
		gcc -fsyntax-only -nostdinc -fno-builtin -Wno-unknown-pragmas -DPLAN9 -Itools/u9 -I.front-ref/sys/include -I. $$f || rc=1; \
	done; exit $$rc

clean:
	rm -f test/gridtest test/themetest test/widthtest

# every libcell.a member must define only cell* / Cell* linker symbols
lint:
	@if [ ! -f .front-ref/sys/include/libc.h ]; then echo "lint: skipped (no .front-ref)"; exit 0; fi; \
	d=$$(mktemp -d); rc=0; \
	for f in $(filter-out eaw,$(PURE)) $(IMPURE); do \
		gcc -c -nostdinc -fno-builtin -Wno-unknown-pragmas -DPLAN9 -Itools/u9 -I.front-ref/sys/include -I. $$f.c -o $$d/$$f.o || rc=1; \
	done; \
	bad=$$(nm -g --defined-only $$d/*.o | awk 'NF==3 {print $$3}' | grep -vE '^(cell|Cell)'); \
	if [ -n "$$bad" ]; then echo "lint: symbols without cell prefix:"; echo "$$bad"; rc=1; else echo "lint: ok"; fi; \
	rm -rf $$d; exit $$rc

.PHONY: all test check9 lint clean

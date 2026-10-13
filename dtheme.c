/* dtheme.c: cellloadtheme, read a theme file (impure: open/read/close) and parse it. */
#include "compat.h"
#include <draw.h>
#include "cell.h"

enum { Thememax = 64*1024 };

int
cellloadtheme(Cell *c, char *path)
{
	char *buf;
	long n;
	int fd, rc;
	long tot, i;

	if(path == nil){
		werrstr("theme: nil path");
		return -1;
	}
	fd = open(path, OREAD);
	if(fd < 0){
		werrstr("theme: %.64s: %r", path);
		return -1;
	}
	buf = malloc(Thememax + 1);
	if(buf == nil){
		close(fd);
		werrstr("theme: out of memory");
		return -1;
	}
	tot = 0;
	for(;;){
		n = read(fd, buf + tot, Thememax + 1 - tot);
		if(n < 0){
			werrstr("theme: %.64s: %r", path);
			goto fail;
		}
		if(n == 0)
			break;
		tot += n;
		if(tot > Thememax){
			werrstr("theme: %.64s: file too large", path);
			goto fail;
		}
	}
	close(fd);
	for(i = 0; i < tot; i++)
		if(buf[i] == '\0'){
			werrstr("theme: %.64s: contains NUL byte", path);
			free(buf);
			return -1;
		}
	buf[tot] = '\0';
	rc = cellloadthemestr(c, buf);
	free(buf);
	return rc;
fail:
	close(fd);
	free(buf);
	return -1;
}

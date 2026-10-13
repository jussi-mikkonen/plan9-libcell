/* width.c: pure fixed-width decision over measured ASCII advances. */
#include "compat.h"
#include "cellcore.h"

/*
 * w[i] is the measured advance of rune 0x20+i. All equal and positive:
 * *cw = that width, return 0. Otherwise write a "not fixed-width" error to msg
 * (name cut to 64 bytes so the widths always fit ERRMAX) and return -1.
 */
int
widthcheck(char *font, int *w, int n, int *cw, char *msg, int nmsg)
{
	int i, min, max;

	min = max = n > 0 ? w[0] : 0;
	for(i = 1; i < n; i++){
		if(w[i] < min)
			min = w[i];
		if(w[i] > max)
			max = w[i];
	}
	if(n > 0 && min == max && min > 0){
		*cw = min;
		return 0;
	}
	lsnprint(msg, nmsg, "cellopen: %.64s not fixed-width: ascii widths %d..%d", font, min, max);
	return -1;
}

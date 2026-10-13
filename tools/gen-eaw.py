#!/usr/bin/env python3
"""gen-eaw.py: write eaw.c (the W/F East-Asian-Width table) to stdout.

Linux only; 9front never runs this. Usage:
    python3 tools/gen-eaw.py /usr/share/unicode/ucd/EastAsianWidth.txt > eaw.c
"""
import sys


def main():
    path = sys.argv[1]
    version = ""
    rng = []
    with open(path, encoding="utf-8") as f:
        for i, line in enumerate(f):
            line = line.rstrip("\n")
            if i == 0:
                version = line.lstrip("# ").strip()
            line = line.split("#", 1)[0].strip()
            if not line:
                continue
            a, cls = [x.strip() for x in line.split(";")]
            if cls not in ("W", "F"):
                continue
            if ".." in a:
                lo, hi = [int(x, 16) for x in a.split("..")]
            else:
                lo = hi = int(a, 16)
            rng.append((lo, hi))
    rng.sort()
    merged = []
    for lo, hi in rng:
        if merged and lo <= merged[-1][1] + 1:
            if hi > merged[-1][1]:
                merged[-1] = (merged[-1][0], hi)
        else:
            merged.append((lo, hi))
    o = []
    o.append("/* eaw.c: East-Asian-Width W and F ranges. GENERATED, do not edit.")
    o.append(" *")
    o.append(" * Source: %s" % version)
    o.append(" * Command: python3 tools/gen-eaw.py /usr/share/unicode/ucd/EastAsianWidth.txt > eaw.c")
    o.append(" *")
    o.append(" * A test oracle, not linked into libcell.a (the measured width always wins). */")
    o.append('#include "compat.h"')
    o.append('#include "cellcore.h"')
    o.append("")
    o.append("typedef struct Eaw Eaw;")
    o.append("struct Eaw {")
    o.append("\tRune lo, hi;")
    o.append("};")
    o.append("")
    o.append("static Eaw eaw[] = {")
    for lo, hi in merged:
        o.append("\t{0x%04X, 0x%04X}," % (lo, hi))
    o.append("};")
    o.append("")
    o.append("/* 2 for a W or F rune, else 1 */")
    o.append("int")
    o.append("eawcells(Rune r)")
    o.append("{")
    o.append("\tint lo, hi, m;")
    o.append("")
    o.append("\tlo = 0;")
    o.append("\thi = (int)(sizeof eaw / sizeof eaw[0]) - 1;")
    o.append("\twhile(lo <= hi){")
    o.append("\t\tm = (lo + hi) / 2;")
    o.append("\t\tif(r < eaw[m].lo)")
    o.append("\t\t\thi = m - 1;")
    o.append("\t\telse if(r > eaw[m].hi)")
    o.append("\t\t\tlo = m + 1;")
    o.append("\t\telse")
    o.append("\t\t\treturn 2;")
    o.append("\t}")
    o.append("\treturn 1;")
    o.append("}")
    o.append("")
    o.append("/* range count when sorted and non-overlapping, else -1 */")
    o.append("int")
    o.append("eawcheck(void)")
    o.append("{")
    o.append("\tint i, n;")
    o.append("")
    o.append("\tn = (int)(sizeof eaw / sizeof eaw[0]);")
    o.append("\tfor(i = 0; i < n; i++){")
    o.append("\t\tif(eaw[i].lo > eaw[i].hi)")
    o.append("\t\t\treturn -1;")
    o.append("\t\tif(i + 1 < n && eaw[i].hi >= eaw[i+1].lo)")
    o.append("\t\t\treturn -1;")
    o.append("\t}")
    o.append("\treturn n;")
    o.append("}")
    sys.stdout.write("\n".join(o) + "\n")


main()

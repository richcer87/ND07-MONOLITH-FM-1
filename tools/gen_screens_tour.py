#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""tests/scripts/screens.txt from firmware/src/nd07.c: every page and knob set in order, a note held, a shot each
(the screen review of a theme). Run again when pages or knob sets change."""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
src = (ROOT / "firmware/src/nd07.c").read_text()
pp = src[src.index("PAGE_P[NPAGE]"):]
pp = pp[:pp.index("};")]
rows = re.findall(r'\{(\{[^{}]*\}(?:,\s*\{[^{}]*\})*)\}', pp)
nsub = [max(1, sum(1 for s in re.findall(r'\{([^{}]*)\}', r) if any(x.strip() != "NO" for x in s.split(",")))) for r in rows]
grp = [int(x) for x in re.search(r'PAGE_GRP\[NPAGE\] = \{([^}]*)\}', src).group(1).split(",")]
btn = {"BTN_HOME": 8, "BTN_ARP": 10, "BTN_ENV": 4, "BTN_EDIT": 6, "BTN_GLO": 7, "BTN_FX": 2, "BTN_SEQ": 11, "BTN_LFO": 5}
gb = [btn[x.strip()] for x in re.search(r'GRP_BTN\[NGRP\] = \{([^}]*)\}', src).group(1).split(",")]
assert len(nsub) == len(grp), (len(nsub), len(grp))
t, out = 300, ["100 keys 0x80"]
for pg, n in enumerate(nsub):
    for sb in range(n):
        if pg or sb:
            out.append(f"{t} btn {gb[grp[pg]]}"); t += 250
        out.append(f"{t} shot"); t += 150
out.append(f"{t} end")
(ROOT / "tests/scripts/screens.txt").write_text(
    "# every page and knob set, a note held (screens for the theme check); made by tools/gen_screens_tour.py\n" + "\n".join(out) + "\n")
print(len(nsub), "pages,", sum(nsub), "screens")

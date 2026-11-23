#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""web/install.html from web/install.src.html with web/fm1pkg.js and web/fm1ota.js inside (no imports: the page works
from GitHub Pages and opened from the disk). --check: fail when install.html is not what this would write."""
import re
import sys
from pathlib import Path

WEB = Path(__file__).resolve().parents[1] / "web"


def inline(name):
    js = (WEB / name).read_text()
    js = re.sub(r"^import .*?;\n", "", js, flags=re.M)
    return f"/* ---- {name} (inlined by tools/make_install_page.py) ---- */\n" + re.sub(r"^export ", "", js, flags=re.M)


def page():
    src = (WEB / "install.src.html").read_text()
    return re.sub(r"/\*@INLINE ([\w.]+)\*/", lambda m: inline(m.group(1)), src)


if __name__ == "__main__":
    out = page()
    if "--check" in sys.argv:
        if (WEB / "install.html").read_text() != out:
            sys.exit("web/install.html is stale: run python3 tools/make_install_page.py")
        print("web/install.html is current")
    else:
        (WEB / "install.html").write_text(out)
        print("wrote web/install.html")

"""usage: python _setp.py board REF X Y [ROT] ...   (edits PLACE in layout_<board>.py)"""
import re, sys
board = sys.argv[1]
p = f"layout_{board}.py"
t = open(p, encoding="utf-8").read()
a = sys.argv[2:]
i = 0
while i < len(a):
    ref, x, y = a[i], a[i+1], a[i+2]
    i += 3
    rot = None
    if i < len(a) and re.fullmatch(r"r\d+", a[i]):
        rot = a[i][1:]; i += 1
    m = re.search(r'"%s": \(([-\d.]+), ([-\d.]+), (\d+)\)' % re.escape(ref), t)
    if not m:
        print("missing", ref); continue
    t = t[:m.start()] + '"%s": (%s, %s, %s)' % (ref, x, y, rot or m.group(3)) + t[m.end():]
open(p, "w", encoding="utf-8").write(t)

"""List library symbols whose name contains a text:  python findsym.py MCP2551"""
import glob, os, re, sys
root = r"D:\KiCad\10.0\share\kicad\symbols"
q = sys.argv[1].lower()
for f in glob.glob(os.path.join(root, "*.kicad_sym")):
    t = open(f, encoding="utf-8").read()
    for m in re.finditer(r'\(symbol "([^"]+)"', t):
        n = m.group(1)
        if q in n.lower() and not re.search(r"_\d+_\d+$", n):
            print(os.path.basename(f)[:-10] + ":" + n)

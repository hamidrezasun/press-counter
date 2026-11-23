"""Print pad layout / bounding box of KiCad library footprints (run with KiCad's python)."""
import sys
import pcbnew

FP_ROOT = r"D:\KiCad\10.0\share\kicad\footprints"


def info(lib, name, show_pads=True):
    fp = pcbnew.FootprintLoad(f"{FP_ROOT}\\{lib}.pretty", name)
    if fp is None:
        print(f"!! {lib}:{name} NOT FOUND")
        return
    bb = fp.GetBoundingBox(False)
    mm = pcbnew.ToMM
    print(f"== {lib}:{name}  bbox x[{mm(bb.GetLeft()):.2f},{mm(bb.GetRight()):.2f}] "
          f"y[{mm(bb.GetTop()):.2f},{mm(bb.GetBottom()):.2f}]  w={mm(bb.GetWidth()):.2f} h={mm(bb.GetHeight()):.2f}")
    if show_pads:
        for p in fp.Pads():
            pos = p.GetPosition()
            sz = p.GetSize()
            dr = p.GetDrillSize()
            print(f"   pad {p.GetNumber():>3} ({mm(pos.x):7.2f},{mm(pos.y):7.2f}) size {mm(sz.x):.2f}x{mm(sz.y):.2f} "
                  f"drill {mm(dr.x):.2f} attr={p.GetAttribute()}")


if __name__ == "__main__":
    for arg in sys.argv[1:]:
        lib, name = arg.split(":", 1)
        info(lib, name)

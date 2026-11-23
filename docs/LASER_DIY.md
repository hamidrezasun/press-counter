# Making the board with a galvo laser (EzCad) and a drill - what works, what does not

Files: [`hardware/fabrication/laser/`](../hardware/fabrication/laser/) (made by `hardware/tools/make_laser_files.py` and `make_laser_test.py`).
Nothing here was tried on a laser: the settings are **starting points to find out on your machine**, not verified values. The EzCad copy on this
PC is only a sample; the laser PC has its own lens, laser type and parameter files.

## 1. Honest summary

* Fine tracks (0.25 mm with 0.2 mm gaps) are within reach of a fibre / MOPA galvo laser. The 5 mm mains gaps are only wide areas to burn.
* **This board was designed for a factory process with plated holes.** A laser and a drill cannot plate holes, and that costs a lot of work:
  * **137 vias** (125 vias + 12 thermal vias under the ESP32) have to be made by hand: drill 0.4 mm, push a 0.3 mm tinned copper wire through, solder both sides, cut. About 2-3 hours per board.
  * **74 through-hole pads connect to copper on the top layer** (17 of them on both layers). With a plain hole the lead is soldered on the bottom only, so the top copper does not touch the lead. Where the pad lies under the part body (relays K1-K5: 25 pads, PS1: 3, the sockets P1 / P2, terminal blocks, the fuse holder) the top side cannot be soldered at all. Workarounds: a short wire from the lead's top copper to a bottom pad, or re-routing those pads to the bottom layer.
  * No solder mask: bare copper oxidises and bridges when soldering. Spray lacquer after assembly.
* **For a prototype** (1-5 boards, to try the circuit): possible, with the hand work above.
* **For production**: use a board house. eca.ir has a printed-circuit-board ordering service (footer of the shop, "سفارش و ساخت برد مدار چاپی جدید"); ask for a 2-layer, 1.6 mm, plated-through-hole quote with the Gerber zip. Plated holes, solder mask and silk are the difference between a hand-made and a mass-produced board, and a 202 x 104 mm board is cheap in a normal fab.
* **A better laser project for this design**: a single-sided or a DIY-routed variant (all through-hole pads connected on the bottom, no vias, jumper wires where needed). I can generate it on request; it would be a different layout (and needs its own checks), not this file.

## 2. The files

| File | Use |
|------|-----|
| `top_remove.bmp` | 8084 x 4164 px, 1016 dpi (25 um per pixel) = 202.1 x 104.1 mm. **Black = copper the laser must remove**, seen from the top. White = tracks, pads, ground pour (kept). A 0.25 mm pilot pit sits in every hole centre; a 0.1 mm outline marks the board edge |
| `bottom_remove.bmp` | the same for the bottom, **mirrored left-right**: flip the board about its vertical axis and it lines up |
| `drill_list.csv`, `drill_summary.txt` | 230 holes: x, y (mm from the top-left corner, board seen from the top), diameter, type, reference |
| `test_pattern.bmp` | 50 x 36 mm calibration piece: lines and gaps 0.15-0.5 mm, a pad grid with pilot pits, a 5 mm isolation channel, a ruler. **Do this first on a scrap piece** |

Burnt area: about 34 % of the top and 32 % of the bottom (about 70 cm2 per side).

## 3. Two ways to use the laser

| | A. Laser on resist, then etch (recommended) | B. Direct copper ablation |
|--|---------------------------------------------|---------------------------|
| How | coat the copper with matt black spray paint (thin, even), the laser removes the paint where copper must go, etch in ferric chloride / sodium persulfate / HCl + H2O2, strip the paint with acetone | the laser burns the 35 um copper away |
| Needs | any galvo laser that marks paint (even a low-power one) + chemistry, ventilation | a laser that ablates bare copper cleanly (fibre / MOPA, typically 20 W or more); many passes |
| Time | one fast pass per side | many passes (burnt area about 70 cm2 per side), long |
| Risks | undercut at 0.2 mm gaps (test), etch time | **charred FR4 in the burnt areas is slightly conductive**; this board carries 230 V: after ablation brush, wash with isopropanol and check with a megohmmeter (>= 100 Mohm at 500 V between the mains and the low-voltage areas); fumes |
| 230 V areas | fine (the 5 mm gaps are clean copper-free laminate) | needs the cleaning and the check above |

Whatever the method: **fume extraction and laser-safety glasses for the wavelength** (1064 nm for fibre). Burning epoxy glass and copper produces toxic fumes.

## 4. Steps

1. **Check the lens field.** The board is 202 x 104 mm. A 110 x 110 mm lens cannot do it in one go; you need a field of 200 x 200 mm or larger, or you tile the job (cut the BMP into 2-3 pieces: tell me the usable field and I will generate the tiles with overlap marks). Measure the real field with a 100 mm square mark on scrap.
2. **Blank**: double-sided FR4 1.6 mm, 35 um copper, cut to 202.1 x 104.1 mm (or larger, nothing is burnt outside the picture). Clean the copper with isopropanol; no fingerprints.
3. **Registration holes first**: the four corner holes (3.2 mm, 4 mm from each edge: x = 4 and 198.1, y = 4 and 100.1 - the pattern is symmetric, so the same four holes work top and bottom). Drill them 3.2 mm; make a work plate with four pins at those positions (or two pins and a corner stop) and use it for the laser, flipped for the bottom.
4. **Top layer**: import `top_remove.bmp` (File > Import / bitmap), set the size by DPI 1016 or 202.1 x 104.1 mm, place the centre of the picture at the centre of the plate. Scan (raster) fill, no inversion if the black pixels are marked (check the preview: the pilot pits must be black).
5. **Test with `test_pattern.bmp` first** (a start only, tune on scrap):

   | Parameter | Resist method (paint) | Copper ablation (fibre / MOPA) |
   |-----------|-----------------------|--------------------------------|
   | Line spacing / dot spacing | 0.02-0.03 mm | 0.01-0.02 mm |
   | Speed | 1000-3000 mm/s | 500-2000 mm/s |
   | Power | 20-60 % of a 20 W | 70-100 % |
   | Frequency | 30-60 kHz | 30-80 kHz (MOPA: pulse width 2-10 ns) |
   | Passes | 1-2 | 5-30 until the copper is gone, no copper bridges at 0.15 mm gaps |

   Pass criteria: the 0.2 mm gap series is open (no copper bridge, check with a meter between the lines), the 0.25 mm lines are intact, the 5 mm channel is clean, the pilot pits are visible. Focus precisely (a wrong focus widens the gaps).
6. **Flip** about the vertical axis on the pins, laser `bottom_remove.bmp`. Check the alignment with a drill through the pilot pits at two far corners before burning the second side if you can.
7. **Etch** (method A) and strip the paint; **clean** and check the gaps in the 230 V zone with a loupe and a meter.
8. **Drill** (see section 5), de-burr with fine sandpaper, tin the copper (tinning solder or a flux-lacquer) before it oxidises.
9. **Vias**: all `via` and the 12 ESP32 thermal vias from `drill_list.csv`, wire both sides.
10. **Solder**: SMD first (paste and hot plate or hot air; the board has all SMD parts on top), then the through-hole parts. See section 1 for the pads that need top and bottom contact.

## 5. Drilling

From `drill_summary.txt`: 0.3 mm x 137 (vias; **use a 0.4 mm bit**, a 0.3 mm bit breaks all the time), 0.6 mm x 4 (varistors RV1, RV2), 1.0 mm x 30, 1.2 mm x 2 (RS1), 1.3 mm x 43 (terminals, relays, PS1, sockets, buzzer), 2.4 x 1.3 slots x 2 (fuse holder F1: drill two 1.3 mm holes and join them with the bit), 3.2 mm x 6 (mounting holes). The micro-USB J3 has 0.8 mm pegs and four small slots for the shield tabs of the Wuerth connector: a generic SMD micro-USB without through-hole tabs does not need them (skip them or drill them as you like).
Drill from the top (the laser pilot pit on the top side marks every centre). Use a carbide bit, a spindle of 20 000+ rpm and a backing board; a hand drill press works for the 1.0-1.3 mm holes. For a CNC, `hardware/fabrication/main/gerbers/*.drl` (Excellon) can be used with any drill-to-G-code program; its origin is the board's drill/place origin, `drill_list.csv` is measured from the top-left corner of the board.

## 6. Checks before power-up (it is a mains board)

* The 5 mm gaps around the 230 V areas are clean: loupe, then isopropanol, then a megohmmeter or at least a high-resistance meter.
* No copper whisker or solder bridge between the 230 V area and anything else; check each via wire.
* First power-up through an isolating transformer with a lamp limiter, enclosure closed (INSTALLATION.md). A hand-made board has not been hipot-tested: treat it as a bench prototype, not as a product.

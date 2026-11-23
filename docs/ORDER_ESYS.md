# Ordering the board at esys.ir (level 1, FR-4)

Upload file: [`hardware/fabrication/esys/press_counter_esys_gerber.zip`](../hardware/fabrication/esys/press_counter_esys_gerber.zip)
(built by `hardware/tools/make_esys_package.py`, which first runs `check_esys.py` against the shop's limits).
Rules read on 2026-10-15 from <https://esys.ir/page.aspx?i=58> (order notes), <https://esys.ir/page.aspx?i=59> (level-1 capability) and
<https://esys.ir/pcb.aspx>. The price calculator is behind the shop login; nothing was submitted, you place the order (login, CAPTCHA, payment).

## 1. Requirements and how the board meets them

| Rule of the shop | This board | |
|------------------|-----------|--|
| File format: only Gerber (or Altium PcbDoc) in a ZIP / RAR; KiCad project files, PDF, images are refused | Gerber ZIP | OK |
| Gerber names must start with G or end in .TXT (usual extensions GTL, GBL, GKO ...) | `Gerber_press_counter.GTL GBL GTS GBS GTO GBO GKO` (G prefix and Protel extension) | OK |
| NC drill file mandatory, extension .TXT | `Gerber_press_counter_PTH.TXT` (plated) and `..._NPTH.TXT` (non-plated), Excellon, mm, 8 + 2 tools, 222 plated + 8 non-plated holes | OK |
| Board outline as a closed line on the keep-out layer; internal cut-outs on it too | `.GKO` = the outline, one closed loop (checked), rounded corners R3; **no internal cut-outs** | OK |
| Size entered in cm with 2 decimals | **20.21 x 10.41 cm** | enter it |
| Holes that must not be plated: pads with "Plated" off | the 8 mounting / peg holes are non-plated pads and sit in the NPTH file | OK |
| Layers FR-4 1-16 | 2 | OK |
| Size 0.6 x 0.6 ... 60 x 50 cm | 20.21 x 10.41 | OK |
| Thickness 0.4-2 mm | 1.6 mm | OK |
| Copper 1 oz / 2 oz outer | 1 oz (the relay tracks are sized for 1 oz) | choose 1 oz |
| Min track 0.127 mm (5 mil) | narrowest 0.187 mm | OK |
| Min spacing 0.127 mm | 0.2 mm design rule, KiCad DRC 0 violations | OK |
| Drill 0.2-6.3 mm | 0.3 ... 3.2 mm (largest 3.2) | OK |
| Via inner >= 0.2 mm, outer >= 0.45 mm | 0.3 / 0.6 mm (125 vias, covered by mask on both sides) | OK |
| Annular ring >= 0.1016 mm | vias 0.15 mm; every plated pad checked | OK |
| Legend (silk) stroke >= 0.153 mm, character height >= 1.016 mm | **fixed for this order**: all texts >= 1.02 mm high, strokes >= 0.16 mm (silk was 0.7-0.9 mm / 0.12-0.15 mm before); the project DRC now enforces it | OK |
| Plated half holes >= 0.6 mm | none | OK |
| Cut-out (routed slot) width >= 1 mm | the micro-USB shield slots (0.55 / 0.85 mm) were made **round holes**; the only slots left are the fuse-holder slots 2.4 x 1.3 mm (1.3 mm wide) | OK |
| Stack-up: the default of the calculator | standard 2 layers, no special stack-up | OK |
| Panelising: send single boards, the shop panelises | single board | OK |
| Silk colour white (only with a white mask: white or black) | white silk on a coloured mask | OK |

`python check_esys.py` (in `hardware/tools`, with KiCad's Python) repeats these checks on the board file.

## 2. Values for the form

| Field | Value |
|-------|-------|
| Material | FR-4 |
| Layers | 2 |
| Size | 20.21 x 10.41 cm |
| Thickness | 1.6 mm |
| Outer copper | 1 oz (35 um) |
| Surface finish | lead-free HASL (cheapest, fine for hand and reflow soldering) or ENIG (flatter pads, dearer; nothing on this board needs it) |
| Solder mask | green (or any dark colour; the shop's table allows black silk only on a white mask, white silk on every other colour) |
| Silkscreen | white |
| Vias | covered with solder mask, not filled (the Gerber already has them tented) |
| Quantity | 5 for the pilot (see PRODUCTION.md: 5 boards, tests, 24 h burn-in before a run). The "special / cheap" programme is only for boards smaller than 10 x 10 cm, not this one |
| Production programme | normal (faster) or scheduled (cheaper, slower) - same quality; the "mass production" programme for large series |
| Serial number | level 1 prints a serial number somewhere on the board. If it must not be there, a higher level (more expensive) is needed - say so in the comment |
| E-test | flying probe / E-test of all nets (the shop's default; ask for it to be included) |

## 3. Comment for the order (copy into the description box)

English:

> 2-layer FR-4 1.6 mm, 1 oz, lead-free HASL, white silkscreen. Gerber files: copper GTL/GBL, mask GTS/GBS, silk GTO/GBO, outline GKO (closed loop, no cut-outs). Drill: PTH and NPTH .TXT (Excellon, mm). Plated slots 2.4 x 1.3 mm at F1 (2 slots). Large copper-free areas (WiFi antenna at the top edge, RFID antenna area, 5 mm mains isolation gaps) must stay copper-free: please do not add copper thieving or fill. Vias are tented. Please run an E-test. Mains voltage on part of the board: please do not alter the clearances.

فارسی:

> برد دو لایه FR-4 با ضخامت 1.6 میلی‌متر، مس 1 اونس، HASL بدون سرب، سیلک سفید. فایل‌ها: مس GTL/GBL، ماسک GTS/GBS، سیلک GTO/GBO، لبه برد GKO (یک حلقه بسته و بدون برش داخلی). فایل دریل PTH و NPTH با پسوند TXT (اکسلون، میلی‌متر). دو اسلات متالیزه 2.4×1.3 میلی‌متر در محل F1. نواحی بدون مس (آنتن WiFi لبه بالای برد، ناحیه آنتن RFID و فاصله‌های 5 میلی‌متری ولتاژ شبکه) باید بدون مس بمانند؛ لطفاً مس اضافی (thieving) اضافه نشود. ویاها با سولدر مسک پوشانده شوند. لطفاً تست الکتریکی (E-test) انجام شود. بخشی از برد ولتاژ شبکه 230 ولت دارد؛ لطفاً فواصل را تغییر ندهید.

## 4. Steps

1. Check the zip once: open it, nine files, names as above. (The shop's own Gerber viewer or any viewer such as the free online Gerber viewers should show the outline, copper, silk and the holes in the right places.)
2. Log in at esys.ir, order "سفارش PCB", enter the values of section 2, upload the ZIP, paste the comment, solve the CAPTCHA, submit. The review takes up to 3 working hours; you then get a pre-invoice in your account. **Production only starts after you pay it**; the price in the pre-invoice is final.
3. Lead time = production + shipping to Iran + customs (working days, Saturday / Sunday not counted); the shop calls you when the boards arrive.
4. Print `hardware/fabrication/check/print_1to1_A4_top.pdf` at 100 % and check the real parts against it **before** you pay (DESIGN_REVIEW.md section 0.5): a wrong footprint is cheaper to fix now.

## 5. What is not guaranteed by a level-1 order

* The shop checks the files against its capability table; it does not check the electronics. DRC / ERC are clean, but the board has never been built (DESIGN_REVIEW.md).
* Solder-mask clearance is as in the Gerber (mask opening = pad size, no extra expansion): a shop may adjust it; this does not matter here (the finest pitch is the ESP32 module's 1.27 mm).
* The board carries 230 V: the 5 mm isolation gaps are copper-free areas and must reach you as designed. Check them on the first board with a loupe; a hipot test is in PRODUCTION.md.

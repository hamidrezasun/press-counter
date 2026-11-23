"""SPICE check of the 230 VAC press-sense input (J2 -> RS1 120k -> MB10S -> C5 / D5 / R12 / PC817 -> IO34), with KiCad's ngspice.

    D:/KiCad/10.0/bin/python.exe hardware/sim/sense_input.py

Cases: normal mains, -15 % mains with a weak optocoupler, and the press switched OFF by a contact with an RC snubber (leakage into the
contactor coil) - with and without R12 (the 2.2k across the opto LED).  Models are simple diode models fitted to typical forward voltages
(red LED 1.8 V, PC817 IR LED 1.1 V at 1 mA); the PC817 output is a current source = CTR x LED current that saturates near 0 V.
This is a design check, not a substitute for measuring the first boards.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ngspice_run import run  # noqa: E402

TEMPLATE = """press sense input
Vac src 0 SIN(0 {vpk} 50)
Vctl ctl 0 PWL(0 0 40m 0 40.1m {on} 240m {on} 240.1m 0)
S1 src a ctl 0 SWM
.model SWM sw(vt=0.5 vh=0.1 ron=1 roff=1e12)
Csn src sn {csn}
Rsn sn a 100
Rcoil a 0 7.6k
RS1 a bra 120k
D1 bra brp DB
D2 0 brp DB
D3 brn bra DB
D4 brn 0 DB
C5 brp brn 22u
D5 brp k5 DRED
Rb k5 brn {rb}
Dpc k5 x DIR
Vi x brn 0
Rf1 brp 0 1G
Rf2 brn 0 1G
Vcc vcc 0 3.3
R1 vcc press {r1}
Bout press 0 I = {ctr}*max(i(Vi),0)*tanh(max(v(press),0)/0.15)
Cp press 0 1n
.model DB D(IS=1e-9 N=1.8 RS=0.05 BV=1000)
.model DRED D(IS=1e-20 N=1.8 RS=10)
.model DIR D(IS=1e-14 N=1.7 RS=5)
.tran 20u 400m uic
.end
"""


def case(name, vrms, ctr, rb=2200, switched=True, csn=1e-15, r1=22e3):
    net = TEMPLATE.format(vpk=vrms * 2 ** 0.5, on=1 if switched else 0, csn=csn, rb=rb, ctr=ctr, r1=r1)
    r = run(net, ["time", "v(press)", "i(vi)"])
    t, vp, il = r["time"], r["v(press)"], r["i(vi)"]

    def window(t0, t1):
        return [i for i, x in enumerate(t) if t0 <= x <= t1]

    on = window(0.14, 0.24)                     # steady state while the press voltage is on
    off = window(0.33, 0.40)                    # long after it was switched off
    led_on = sum(il[i] for i in on) / len(on) * 1e3
    p_on = max(vp[i] for i in on)                # worst point of the 100 Hz ripple
    p_off = min(vp[i] for i in off)
    fall = next((t[i] for i in range(len(t)) if t[i] > 0.04 and vp[i] < 0.8), None)
    rise = next((t[i] for i in range(len(t)) if t[i] > 0.24 and vp[i] > 2.5), None)
    print(f"{name:52s} LED {led_on:5.2f} mA   IO34 on {p_on:4.2f} V  off {p_off:4.2f} V"
          + (f"   on after {(fall - 0.04) * 1e3:5.1f} ms" if fall and switched else "")
          + (f"   off after {(rise - 0.24) * 1e3:5.1f} ms" if rise and switched else ""))
    return p_on, p_off


if __name__ == "__main__":
    print("IO34 reads LOW below 0.8 V and HIGH above 2.5 V (ESP32 at 3.3 V); between them the level is undefined.")
    print("Board values: RS1 120k, R12 2.2k across the opto LED, R1 22k pull-up.\n")
    case("230 VAC, CTR 100 %", 230, 1.0)
    case("253 VAC (+10 %), CTR 100 %", 253, 1.0)
    case("195 VAC (-15 %), CTR 50 %", 195, 0.5)
    case("195 VAC (-15 %), CTR 35 % (worst PC817 at 1 mA)", 195, 0.35)
    print()
    for csn in (47e-9, 100e-9, 220e-9):
        case(f"press OFF, {csn * 1e9:.0f} nF snubber leaks into the coil", 230, 1.0, switched=False, csn=csn)
    print()
    print("for comparison, the v6.1 values (no R12, 10k pull-up):")
    case("  195 VAC (-15 %), CTR 35 %", 195, 0.35, rb=1e12, r1=10e3)
    case("  press OFF, 100 nF snubber leaks", 230, 1.0, rb=1e12, r1=10e3, switched=False, csn=100e-9)

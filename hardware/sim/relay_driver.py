"""SPICE check of one relay driver (IO -> 1k -> SS8050, 10k base pull-down, T73 5 V coil 70 ohm, 1N4007 flyback, LED + 1k across the coil).

    D:/KiCad/10.0/bin/python.exe hardware/sim/relay_driver.py

Question: can a relay pull in while the ESP32 starts?  A T73 needs >= 75 % of 5 V (54 mA) to pull in and releases below 10 % (7 mA).
The ESP32 relay pins are high-impedance until the firmware drives them LOW; the pessimistic case gives the pin a weak internal pull-up.
Generic SS8050 model (hFE ~200); a design check, not a measurement.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ngspice_run import run  # noqa: E402

MODELS = """
.model SS8050 NPN(IS=2e-14 BF=200 VAF=100 IKF=0.6 ISE=1e-14 NE=1.5 BR=5 RB=8 RC=0.25 CJE=25p CJC=12p TF=0.6n TR=40n)
.model M7 D(IS=5e-9 N=1.9 RS=0.04 TT=2u CJO=15p BV=1000)
.model LEDG D(IS=1e-20 N=1.9 RS=10)
"""

BASE = """relay driver
V5 v5 0 5
Lc v5 c1 80m
Rc c1 c 70
D12 c v5 M7
R43 v5 la 1k
D13 la c LEDG
Q2 c b 0 SS8050
R13 b 0 10k
R9 pin b 1k
{drive}
{models}
{analysis}
.end
"""


def dc(name, drive):
    r = run(BASE.format(drive=drive, models=MODELS, analysis=".op"), ["i(v5)", "v(b)", "v(c)"])
    i_coil = -r["i(v5)"][0] * 1e3
    print(f"{name:56s} base {r['v(b)'][0]:4.2f} V   coil+LED current {i_coil:6.2f} mA   collector {r['v(c)'][0]:4.2f} V")


if __name__ == "__main__":
    print("T73 5 V relay: pulls in at >= 54 mA, drops out below 7 mA.\n")
    dc("pin high-impedance (reset, ROM boot)", "")
    dc("pin high-impedance with a weak internal pull-up (45k)", "V33 v33 0 3.3\nRpu v33 pin 45k")
    dc("pin high-impedance with a strong 10k pull-up (fault)", "V33 v33 0 3.3\nRpu v33 pin 10k")
    dc("pin driven LOW (firmware: relay off)", "Vp pin 0 0")
    dc("pin driven HIGH 3.3 V (relay on)", "Vp pin 0 3.3")
    dc("pin HIGH but only 3.0 V (weak supply)", "Vp pin 0 3.0")
    r = run(BASE.format(drive="Vp pin 0 PWL(0 3.3 20m 3.3 20.01m 0)", models=MODELS, analysis=".tran 1u 30m uic"), ["time", "v(c)"])
    peak = max(r["v(c)"])
    print(f"\nswitch-off spike at the collector: {peak:.2f} V (SS8050 VCEO = 25 V) - the flyback diode clamps it to 5 V + one diode drop")

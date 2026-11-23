@echo off
rem Full pipeline (about 15-30 minutes, most of it Freerouting).  Needs KiCad 10 and Java (see route_fr.py: JAVA25, FR2_JAR).
rem   1 schematic + ERC   2 placement, own mains router, Freerouting   3 island repair   4 zone fill + DRC   5 dangling-copper cleanup
rem   6 silk rebuild   7 final zone fill + DRC + gerbers / drill / position files / PDFs / renders into ..\fabrication\main\
cd /d "%~dp0"
set PY=D:\KiCad\10.0\bin\python.exe
set PYTHONIOENCODING=utf-8
"%PY%" -P -u gen_all.py main || exit /b 1
"%PY%" -P -u route_fr.py main --passes 60 > "..\main\route.log" 2>&1 || exit /b 1
for %%N in (+5V +3V3 RLY_N GND) do "%PY%" -P -u repair.py main %%N --apply --inplace
"%PY%" -P -u finish.py main --no-export
"%PY%" -P -u cleanup.py main
"%PY%" -P -u resilk.py main
"%PY%" -P -u finish.py main

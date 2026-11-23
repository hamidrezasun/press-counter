@echo off
rem Full pipeline for both boards (about 25 minutes, most of it Freerouting).  Needs KiCad 10 and Java (see route_fr.py: JAVA, FR_JAR).
rem   1 schematic + ERC   2 placement, own mains router, Freerouting   3 island repair (+3V3)   4 zone fill + DRC   5 dangling-copper cleanup
rem   6 silk rebuild   7 final zone fill + DRC + gerbers / drill / position files / PDFs / renders into ..\fabrication\<board>\
cd /d "%~dp0"
set PY=D:\KiCad\10.0\bin\python.exe
set PYTHONIOENCODING=utf-8
"%PY%" -P -u gen_all.py all || exit /b 1
for %%B in (panel main) do (
  "%PY%" -P -u route_fr.py %%B --passes 60 > "..\%%B\route.log" 2>&1 || exit /b 1
)
"%PY%" -P -u repair.py main +3V3 --apply --inplace
for %%B in (panel main) do (
  "%PY%" -P -u finish.py %%B --no-export
  "%PY%" -P -u cleanup.py %%B
  "%PY%" -P -u resilk.py %%B
  "%PY%" -P -u finish.py %%B
)

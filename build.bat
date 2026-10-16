@echo off
rem Builds Megabonk 3DS from your own copy of Megabonk. See README.md.
where py >nul 2>nul && (py -3 "%~dp0build_megabonk3ds.py" %*) || (python "%~dp0build_megabonk3ds.py" %*)
pause

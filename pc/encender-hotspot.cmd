@echo off
rem ==========================================================
rem  ENCIENDE EL HOTSPOT DEL PC (red "HONOR ola" / quesesoo)
rem  ParkNet (el Arduino) se une solo a esta red, y a traves
rem  de esta red llega al broker publico de internet.
rem  Doble clic y cerrar: el hotspot queda prendido igual.
rem ==========================================================
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0hotspot.ps1"
echo.
pause

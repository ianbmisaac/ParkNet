@echo off
rem ==========================================================
rem  ParkNet - arranca el servidor local del panel
rem  (despues abre http://localhost:8765/dashboard.html)
rem ==========================================================
title ParkNet - servidor local
cd /d "%~dp0"

echo.
echo   Arrancando el servidor de ParkNet...
echo   (para parar: cierra esta ventana o pulsa Ctrl+C)
echo.
start "" "http://localhost:8765/dashboard.html"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0serve.ps1"

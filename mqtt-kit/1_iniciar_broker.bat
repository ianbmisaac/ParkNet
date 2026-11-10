@echo off
rem ==========================================================
rem  1) INICIAR EL BROKER (dejar esta ventana abierta siempre)
rem  Si Mosquitto esta en otra ruta, edita la linea MOSQ=
rem ==========================================================
title Mosquitto - broker MQTT (no cerrar)
set MOSQ=C:\Program Files\mosquitto
cd /d "%MOSQ%"

echo.
echo   Broker MQTT escuchando en el puerto 1883 (verbose)
echo   Deja esta ventana abierta. Para parar: Ctrl+C
echo.
mosquitto -c "%~dp0mosquitto.conf" -v

pause

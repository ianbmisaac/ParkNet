@echo off
rem ==========================================================
rem  3) MOVER EL SERVO (ventana que manda comandos)
rem  ABRIR / CERRAR o un angulo 0-180
rem  Cambia "grupo1" por el nombre asignado a tu proyecto.
rem ==========================================================
title Mosquitto - comandos al servo
set MOSQ=C:\Program Files\mosquitto
cd /d "%MOSQ%"
echo.
echo   Comandos: ABRIR   CERRAR   o un angulo (ej: 45). Ctrl+C para salir.
echo   (si el hardware es ParkNet: ABRIR/CERRAR mueven la barrera de verdad)
echo.
:sigue
set /p CMD="> "
mosquitto_pub -h localhost -t "iot/grupo1/servo" -m "%CMD%"
goto sigue

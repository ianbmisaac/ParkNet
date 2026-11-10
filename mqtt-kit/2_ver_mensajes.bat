@echo off
rem ==========================================================
rem  2) VER TODOS LOS MENSAJES DEL GRUPO (ventana suscriptor)
rem  Cambia "grupo1" por el nombre asignado a tu proyecto.
rem ==========================================================
title Mosquitto - ver mensajes
set MOSQ=C:\Program Files\mosquitto
cd /d "%MOSQ%"
mosquitto_sub -h localhost -t "iot/grupo1/#" -v

pause

# Proyecto IoT - Arduino UNO R4 WiFi + Mosquitto

Contenido:
- `arduino_r4_mqtt/arduino_r4_mqtt.ino`: codigo para el Arduino UNO R4 WiFi.
- `mosquitto/`: configuracion del broker y scripts de prueba para Windows.

## 1. Servidor (Mosquitto en tu PC)
1. Instala Mosquitto (broker y clientes) desde mosquitto.org.
2. Copia `mosquitto/mosquitto.conf` donde quieras (los .bat lo usan desde su carpeta).
   Contenido: `listener 1883` y `allow_anonymous true` (sin esto, Mosquitto 2.x rechaza conexiones externas).
3. Ejecuta `1_iniciar_broker.bat` y deja la ventana abierta.
   Si instalaste Mosquitto en otra ruta, edita la linea `cd /d` de los .bat.
4. Averigua tu IP con `ipconfig` (IPv4).
5. Permite el puerto TCP 1883 en el Firewall de Windows.
6. PC y Arduino deben estar en la misma red Wi-Fi de 2.4 GHz.
   Si la red del lab aisla dispositivos, usa el hotspot del celular.

## 2. Arduino
1. Arduino IDE: instala el paquete "Arduino UNO R4 Boards" y selecciona "Arduino UNO R4 WiFi".
2. Instala las bibliotecas: PubSubClient y DHT sensor library (con Adafruit Unified Sensor).
3. Abre el .ino y cambia: ssid, pass, mqtt_server (IP del PC), CLIENT_ID y los topicos ("grupo1" -> nombre de tu proyecto).
4. Sube el codigo y abre el Monitor Serie a 115200.

## 3. Conexiones
| Componente | Cable  | Pin R4 |
|------------|--------|--------|
| DHT22      | VCC    | 5V |
| DHT22      | GND    | GND |
| DHT22      | DATA   | 2 (10 kOhm a VCC si es de 4 pines sueltos) |
| Servo      | Rojo   | 5V (fuente externa si es grande) |
| Servo      | Marron/Negro | GND |
| Servo      | Naranja/Amarillo | 9 (guía) · **A0 en el parking Fixmol** (ya en el .ino) |

## 4. Probar
- `1_iniciar_broker.bat`: arranca el broker (ventana del servidor, no cerrar).
- `2_ver_mensajes.bat`: ves temperatura y humedad cada 5 s.
- `3_mover_servo.bat`: escribe ABRIR, CERRAR o un angulo 0-180 y Enter.
- Comandos validos en `iot/grupo1/servo`: ABRIR, CERRAR o un angulo 0-180.

## 4-bis. Servo en ParkNet: A0 con la fuente externa

Con el hardware Fixmol/ParkNet: el servo va en A0 (naranja), el +5V viene
de la fuente externa (riel rojo) y el GND se une al rail azul (masa comun).
En `arduino_r4_mqtt.ino` ya esta puesto `SERVOPIN A0` por eso.
La barrera del parking responde igual: ABRIR abre, CERRAR cierra.

## 5. Problemas comunes
- MQTT estado=-2: IP incorrecta, firewall bloqueando 1883 o Mosquitto sin el .conf.
- No conecta a Wi-Fi: la red debe ser 2.4 GHz y la clave correcta.
- Servo tiembla o reinicia la placa: usa fuente externa y une los GND.

## 6. Pendiente segun la pauta del Avance III
Base de datos y dashboard (por ejemplo Node-RED), evidencias (fotos, capturas, registro de pruebas) e informe.

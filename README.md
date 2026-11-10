# 🅿️ ParkNet — Estacionamiento inteligente (Equipo Fixmol)

Proyecto IoT para la **Feria de Informática 2026**: parking de 2 plazas con
Arduino UNO R4 WiFi. Sensor IR en plaza 1, plaza 2 virtual desde la web,
barrera con servo, LCD 1602 y LEDs — **operado desde el teléfono sin PC**
y **integrado a MQTT** (Mosquitto) para la arquitectura IoT del curso.

## Repo / quién hace qué

| Área | Responsable | Carpeta |
|---|---|---|
| Arduino / hardware / MQTT | (tú) | `firmware/` + `mqtt-kit/` |
| Página del cliente (front) | amigo 2 | `web/dashboard.html` |
| Base de datos (Firebase) | amigo 1 | (repo del amigo — este repo publica los datos) |

## Estructura

```
ParkNet/
├── firmware/parknet/    ← sketch Arduino (parknet.ino + cabeceras, abre en Arduino IDE)
├── web/dashboard.html   ← la página del sistema (la misma que sirve el Arduino)
├── pc/                  ← servidor local del PC (serve.ps1) y arranques
├── tools/               ← scripts de compilación, diagrama y panel embebido
├── simulacion/          ← diagrama Wokwi (diagram.json)
├── mqtt-kit/            ← broker Mosquitto + comandos de prueba (cualquier PC)
├── docs/                ← README técnico completo del proyecto
└── evidencias/          ← bitácora y capturas para la pauta
```

## Arranque rápido (3 formas)

**A) Solo teléfono y Arduino — sin PC, sin internet**
1. Enciende el Arduino (cualquier fuente).
2. Conecta el teléfono a la WiFi `ParkNet` / clave `parknet2026`.
3. Abre `http://192.168.4.1` → panel completo: plazas, barrera, LCD.

**B) Con red del grupo (hotspot del celular o PC)**
1. El Arduino se une solo a la red (nombre y clave ya grabados en `firmware/parknet/parknet_mqtt.h` / wifi).
2. Abre la IP del Arduino en el navegador.

**C) PC con broker (MQTT, para la pauta del curso)**
1. `cd mqtt-kit` → instalar Mosquitto → doble clic `1_iniciar_broker.bat`.
2. En `firmware/parknet/parknet_mqtt.h` poner la IP del PC que corre el broker (ipconfig).
3. Doble clic `2_ver_mensajes.bat` (ver) y `3_mover_servo.bat` (ABRIR/CERRAR la barrera real).

## Estados del sistema (probado 10-nov-2026)

| Prueba | Resultado |
|---|---|
| Subida de firmware con MQTT | ✅ 43% flash / 32% RAM |
| Red propia `ParkNet` cuando no hay red del grupo | ✅ levanta sola |
| Plaza 2 (web), barrera, LCD desde la página | ✅ responden a la primera |
| MQTT: `iot/grupo1/servo` → `ABRIR`/`CERRAR` | ✅ **mueve la barrera física** |
| MQTT: publica `iot/grupo1/estado` + `iot/grupo1/plazas` | ✅ al cambiar y cada 30 s |
| Ver detalle completo | `docs/README-proyecto.md` |

## Para los compañeros

- Datos que este lado entrega (por MQTT): JSON en `iot/grupo1/estado`
  (`{"libres", "total", "ocupadas", "barrera", "lcd"}`) y el número en
  `iot/grupo1/plazas`. Eso es lo que Firebase/la página del cliente
  pueden consumir.
- Cambios aquí → `git pull` → mismo firmware. Los topics se cambian en
  `firmware/parknet/parknet_mqtt.h` (líneas CAMBIAR AQUI).

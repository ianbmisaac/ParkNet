# Bitácora de evidencias — ParkNet

Registros de pruebas reales del sistema (die: 10-nov-2026, Preavance II de la pauta).

## 1. Firmware con MQTT integrado

- Subida a la placa: `Done in 7.014 seconds` — UNO R4 WiFi, COM3.
- Memoria: sketch 114000 bytes (43% flash), variables globales 10580 bytes (32% RAM).
- MQTT viaja solo en la R4 (`#if USAR_WIFI && !defined(__AVR__)`): si se
  compila para UNO clásico, compila sin MQTT (45% flash).

## 2. WiFi

| Prueba | Resultado |
|---|---|
| Escaneo de redes | ✅ se ven 5-8 redes |
| Red del grupo "HONOR ola" | (apagada ese día) |
| Red propia de respaldo `ParkNet` / `parknet2026` | ✅ levanta sola a los 30 s → `http://192.168.4.1` |
| Unión a hotspot del PC con el mismo SSID/clave | ✅ "tu red ha aparecido: me uno..." → conectado |

## 3. Panel web servido POR el Arduino

- HTTP 200, 28211 bytes, la misma `dashboard.html` del PC (panel embebido en el firmware via `parknet_panel.h`).
- Modo red propia: `http://192.168.4.1` (teléfono entra directo).
- Modo red ajena: panel en la IP del Arduino en esa red (p. ej. `http://192.168.137.80` en el hotspot del PC).

## 4. Hardware real

| Comando | Respuesta del hardware |
|---|---|
| Toggle plaza 2 (web) | ✅ ocupa/libera, LEDs + LCD + contador al instante |
| Barrera ABIERTA/CERRADA | ✅ servo gira (comando web y comando MQTT ambos) |
| LCD modo "fixmol"/"datos" | ✅ |
| Sensor KY-033 plaza 1 | ✅ apertura/cierre de plaza reportado por COM |

## 5. Ciclo MQTT completo (demo de la pauta)

| Paso | Evidencia del log COM |
|---|---|
| Broker Mosquitto corriendo en el PC (192.168.137.1) | servicio Windows "mosquitto" Running; escucha `0.0.0.0:1883`; regla firewall TCP 1883 |
| Arduino se conecta | `[MQTT] conectado y suscrito a iot/grupo1/servo` |
| PC publica `ABRIR` | `[MQTT] iot/grupo1/servo -> ABRIR` → **barrera ABIERTA** (estado JSON) |
| PC publica `CERRAR` | `[MQTT] iot/grupo1/servo -> CERRAR` → **barrera CERRADA** |
| Arduino publica estado | `iot/grupo1/estado` = `{"libres":2,"total":2,"ocupadas":[0,0],"barrera":"CERRADA","lcd":"datos"}` + `iot/grupo1/plazas` |

## 6. Comportamiento ante falla de comunicación (pauta, prueba integral)

- Sin red del grupo y sin red propia: ParkNet sigue funcionando por sí solo
  (sensor, LEDs, LCD, barrera por USB) — MQTT entra en espera sin errores:
  `[MQTT] sin red del grupo, espero al broker...`
- Al volver la red: se reconecta solo (reintento cada 20 s) y publica el
  estado fresco.
- Limitación documentada: en modo **punto de acceso propio** (red `ParkNet`
  emitida por la placa), la placa NO puede iniciar conexiones hacia sus
  clientes (limitación del radio WiFi del R4). Por eso el broker debe vivir
  en la red ajena a la que la placa se une como cliente. Sin broker, todo
  lo demás sigue igual.

## 7. Estado del equipo (10-nov-2026, Preavance II)

Piezas del grupo correctamente entregadas:

- [x] Pieza 3D / render al 80% (equipo)
- [x] Estructura física estable + cableado ordenado (equipo)
- [x] Tabla de pruebas obligatoria + informe (equipo)

Pendientes restantes:

- [ ] Montaje con fuente externa (pila 9V + módulo buck a 5V) — pieza en camino
- [x] Acceso remoto por internet: broker público `test.mosquitto.org`, topics `iot/fixmol/#`
- [x] Página independiente en GitHub Pages: https://ianbmisaac.github.io/ParkNet/
- [x] Evidencia de friend probando el hardware desde otra parte del mundo (10-nov-2026)
- [x] Red base en casa: hotspot del PC en 2.4 GHz (auto-arranque al iniciar sesión)
- [x] Hallazgo del celular del equipo: hotspot `HONOR ola` emite SOLO 5 GHz →
  invisible para la placa (radar de la placa no lo lista en escaneo, mientras
  el PC sí se une, con Clave WPA2 y gateway 10.89.210.x). Conclusión: no se
  puede por ahí — limitación de banda del punto de acceso del teléfono
  (rénar la banda de 2.4 GHz del hotspot del celu del otro equipo
  funcionaría igual, porque el firmware ya guarda ese nombre/clave)
- [ ] Firebase (amigo 1) — consumir `iot/grupo1/estado`
- [ ] Página cliente (amigo 2)
- [ ] Capturas de: broker `-v` con mensajes, panel en el teléfono, pantalla LCD

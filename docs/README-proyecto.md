# ParkNet 3.0 — Estacionamiento Inteligente (2 plazas)

Sistema IoT del **Equipo Fixmol** (Internet de las Cosas).

```
   SENSOR KY-033 (plaza 1)  ──►  ARDUINO UNO R4  ──►  LED 1  (encendido = LIBRE)
        (interrumpelo)             (o UNO clásico)      LED 2
                                                     Servo = barrera
                                       │   ▲   │
                                       │   │   └── Pantalla LCD 1602 (I2C)  A4 / A5
                                       │   └────── 6 plazas? no: 2 plazas
                                       │
                                  USB / RFC2217 (JSON)
                                       │
                                 dashboard.html  (cualquier navegador)
                                 - mismo estado que la pantalla
                                 - abre/cierra la barrera
                                 - simula el auto de la plaza 2
                                 - cambia la LCD entre DATOS y FIXMOL
```

---

## 1. Componentes y mapa de pines

| Componente | Modelo | Pin del Arduino | Rol |
|---|---|---|---|
| Sensor plaza 1 | KY-033 (IR) | **D13** | `INPUT_PULLUP`. LOW = ve algo delante = plaza **ocupada** |
| LED plaza 1 | LED verde | **D12** | **encendido = plaza libre** (con resistencia 330-470 Ω) |
| LED plaza 2 | LED verde | **D10** | ídem (resistencia 330-470 Ω) |
| Barrera | Servo SG90 | **A0** | 90° abierta / 0° cerrada (5 V **externo**) |
| Pantalla | LCD 1602 + conversor **I2C (PCF8574)** | **A4** = SDA, **A5** = SCL | 4 líneas de información, 2 a la vez y rotando |
| Placa | Arduino UNO R4 WiFi (o UNO clásico) | — | cerebro |

* **Plaza 1**: sensor real. Si le pones algo delante, la plaza sale OCUPADA y su LED se apaga.
* **Plaza 2**: plaza virtual, se simula desde la web (un click en la tarjeta o en "Auto en plaza 2").

### La pantalla LCD 1602

El módulo es un LCD 16x2 con un backpack **PCF8574** (el "conversor I2C"). Al Arduino le llegan **4 cables**:

```
   LCD 1602 (backpack I2C)         ARDUINO          PROTOBOARD
   ┌────────────────┐
   │  VCC  ─────────┼───────────────► rail ROJO  (+5V)
   │  GND  ─────────┼───────────────► rail NEGRO
   │  SDA  ─────────┼───────────────► pin A4  ──► col 55
   │  SCL  ─────────┼───────────────► pin A5  ──► col 56
   └────────────────┘
```

* **Dirección I2C**: por defecto `0x27` (`#define LCD_ADDR` en `parknet.ino`).
  Si no enciende, tu módulo puede ser de `0x3F`: cambia ese `#define` y vuelve a subir.
  La dirección se fija con los jumpers **A0/A1** del backpack (A0 cerrado → 0x27).
* **El I2C va por software** (2 pines, sin librería `Wire`) para no gastar la RAM
  del UNO clásico. Por eso el sketch **no necesita instalar ninguna librería**.

### Qué muestra la LCD

| Modo | Línea 1 (siempre visible) | Línea 2 (rota cada 4 s) |
|---|---|---|
| **DATOS** (por defecto) | `LIBRES: 1/2` → **se actualiza al instante** en cuanto una plaza se ocupa o se libera | `P1: LIBRE` · `P2: OCUPADA` · `BARRERA: CERRADA` |
| **FIXMOL** | `*** FIXMOL ***` | `ESTACIONAMIENTO` · `INTELIGENTE IoT` · `2 PLAZAS` |

Se cambia desde la web con el botón **"Pantalla: DATOS / FIXMOL"** (envía
`{"cmd":"lcd","modo":"fixmol"}`); el botón muestra siempre en qué modo estás.

> ⚠️ **El potenciómetro azul del backpack es el contraste.** Si el módulo
> responde por I2C pero la pantalla sale **en negro**, gira ese potenciómetro
> hasta que aparezca el texto. Es el fallo nº1 de estos módulos y no tiene
> nada que ver con el código.
> Para comprobar si el módulo está vivo: botón **"Test I2C (LCD)"** del panel →
> debe imprimir `ACK en 0x27`.

---

## 2. El diagrama de Wokwi

El diagrama usa **rutas explícitas** (el mini-lenguaje de Wokwi) para que
**ningún cable quede encima de otro**:

```json
[ "uno:13", "bb:2t.a", "gold", [ "v-133.0", "h306.7", "v-8.8" ] ],
```

* Cada tramo es relativo: `v-133` = sube 133 unidades, `h306.7` = derecha 306.7.
* **Nada de `"*"`** (ruta automática): el automático aplana los cables y los deja
  unos encima de otros. Solo se usa para los 4 pines de la LCD, cuya posición
  exacta aún no se conoce.
* Los cables que bajan de las filas de piezas entran en la **banda superior**
  (fila a) y los que suben del header inferior en las **filas c/d**, por las
  columnas de la derecha: los dos grupos no se mezclan.

### Disposición

```
   fila A     SERVO (col 48)
   fila B     PANTALLA LCD 1602
   fila C     KY-033 (col 2)   LED 1 (col 11)   LED 2 (col 26)
   ─────────────────── PROTOBOARD LARGA ───────────────────
   ARDUINO UNO R4 WiFi
```

### Tabla de conexiones de la protoboard

| Desde | Hasta | Qué |
|---|---|---|
| `uno:13` | `bb:2t.a` | sensor plaza 1 (OUT) |
| `uno:12` | `bb:11t.a` | LED plaza 1 (ánodo) |
| `uno:10` | `bb:26t.a` | LED plaza 2 (ánodo) |
| `uno:A0` | `bb:48t.c` | servo (señal) |
| `uno:A4` | `bb:55t.d` | LCD SDA |
| `uno:A5` | `bb:56t.d` | LCD SCL |
| `uno:5V` | `bb:tp.1` | alimentación de la protoboard |
| `uno:GND.1` | `bb:tn.2` | masa de la protoboard |
| `ky1:VCC` / `ky1:GND` | `bb:tp.2` / `tn.4` | alimentación del sensor |
| `led1:C` / `led2:C` | `bb:tn.10` / `tn.25` | cátodos de los LEDs |
| `servo1:V+` / `GND` | `bb:tp.49` / `tn.50` | alimentación del servo |
| `lcd:VCC` / `GND` | `bb:tp.50` / `tn.50` | alimentación de la LCD |

Los rails de Wokwi son redes continuas: `tp.1` y `tp.2` son el mismo nodo.
Cada columna es una red: por eso un cable del Arduino y el del componente que
caen en la **misma columna** quedan unidos (y así funcionan).

### Herramientas del proyecto

| Fichero | Para qué |
|---|---|
| `gen-diagram.ps1` | **genera** `diagram.json` (edítalo y ejecuta este script) |
| `check-cables.ps1` | comprueba que ningún cable cruza otro |
| `draw-diagram.ps1` | dibuja el esquema a un PNG para revisarlo rápido |
| `build.ps1` | compila (y sube) para Wokwi / R4 / ambos |
| `serve.ps1` | servidor local + puente con la placa (ver punto 5) |

```powershell
.\gen-diagram.ps1     # regenera el diagrama
.\check-cables.ps1    # 0 cruces, 0 solapes
.\draw-diagram.ps1    # esquema.png para revisar
```

---

## 3. La protoboard como bus de alimentación

* `tp` = rail positivo (+5 V) · `tn` = rail de masa.
* **El rail rojo (`tp`) lo alimenta la FUENTE EXTERNA** (cargador de móvil 5 V,
  power bank o 4 pilas AA ~6 V): su `+5V` entra en `tp.1` y su GND en `tn.1`.
* **El Arduino NO alimenta el rail**: su pin **5V queda SIN cable** (dos fuentes
  de +5 V en paralelo se pelearían). El Arduino lleva **solo su GND** al rail
  azul/negro (`tn.2`) → **masa común con la fuente externa**.
* Cada componente toma su 5 V del rail `tp` y su masa del rail `tn`.

### Alimentación: lo importante

El servo SG90 pide **100-200 mA a ráfagas**. Si lo alimentas del pin 5 V del
Arduino, el rail se hunde, el micro se reinicia por *brownout* (se apagan los
LEDs) y los sensores dan lecturas falsas. **Con 5 V externo + masa común:**

```
   fuente 5 V externa (cargador USB / power bank)
        +5V ──┬──► rail ROJO de la protoboard   (servo + sensor + LCD)
              └──► V+ (rojo) del SERVO
        GND ──┬──► rail NEGRO de la protoboard
              └──► GND del ARDUINO  (masa común, obligatoria)

   pin 5V del Arduino: SIN cable (queda libre)
   señal del servo:    A0 del Arduino
```

**Montaje físico paso a paso** (con la placa desenchufada):

1. Rojo de la fuente externa → la línea **+** (roja) de la protoboard.
2. Negro/GND de la fuente externa → la línea **−** (azul/negra) de la protoboard.
3. **GND del Arduino** (bloque de alimentación o el de AREF) → también a la
   línea − de la protoboard. Sin esta masa común el servo no obedece a A0
   (la señal no tiene referencia).
4. Rojo del servo → línea + · marrón del servo → línea − · naranja (señal) → A0.
5. Comprueba que el **pin 5V del Arduino no tiene nada enchufado**.

### Qué fuente externa usar (elige UNA)

| Fuente | Cómo se conecta | Notas |
|---|---|---|
| Cargador de móvil 5V / power bank | cable USB cortado: rojo = +, negro = − | La más simple |
| 4 pilas AA en soporte (~6V) | rojo → rail `+`, negro → rail `−` | El servo admite 4.8-6V |
| **Pila cuadrada 9V + módulo buck** | pila → **LM2596/mini560 ajustado a 5.0V** → rail `+`/`−` | Ajustar el potenciómetro a 5.0V **antes** de conectar nada (con voltímetro) |
| Pila de 9V directa al rail | — | **❌ PROHIBIDO**: mata la LCD y el sensor, y el servo no aguanta 9V |

Un condensador de **470-1000 µF** en paralelo con el servo se come el pico de
corriente. Los **6 LEDs** (aquí 2) van con **330-470 Ω** en serie con el ánodo:
con 470 Ω consumen 6 mA cada uno en vez de 15 mA y el rail aguanta mejor.

---

## 4. Protocolo serie (115200 baud, líneas JSON)

**Arduino → PC** (al cambiar y una vez por segundo):

```json
{"t":"state","ocupadas":[0,0],"libres":2,"total":2,
 "barrera":"CERRADA","auto":null,"lcd":"datos"}
```

| Campo | Significado |
|---|---|
| `ocupadas` | array 0/1, 1 = plaza ocupada |
| `libres` | nº de plazas libres |
| `total` | nº de plazas (2) |
| `barrera` | `ABIERTA` / `CERRADA` |
| `auto` | `"in"` / `"out"` / `null` (último cambio en la plaza 2) |
| `lcd` | `datos` / `fixmol` (lo que muestra la pantalla) |

**PC → Arduino** (comandos):

| Comando | Efecto |
|---|---|
| `{"cmd":"toggle","slot":2}` | simula un auto en la plaza 2 (la ocupa o la libera) |
| `{"cmd":"barrera","estado":"ABIERTA"}` | abre la barrera con el servo |
| `{"cmd":"barrera","estado":"CERRADA"}` | cierra la barrera |
| `{"cmd":"lcd","modo":"fixmol"}` | la pantalla pasa a mostrar el nombre del equipo |
| `{"cmd":"lcd","modo":"datos"}` | la pantalla vuelve a los datos |
| `{"cmd":"sensores"}` | suelta los overrides (manda el sensor) |
| `{"cmd":"sync"}` | reenvía el estado |
| `{"cmd":"test_servo"}` | comprueba la señal del servo midiendo el duty cycle |
| `{"cmd":"test_cables"}` | muestra el **valor crudo** de los sensores y prueba LEDs y servo |

---

## 5. Usar el software del PC

```powershell
cd C:\Users\pexji\Downloads\wokwi
.\serve.ps1
```

Y abre **http://localhost:8765/dashboard.html** en **cualquier navegador**
(Chrome, Edge, **Firefox**...). No hace falta instalar nada ni autorizar
puertos: el servidor local es quien habla con la placa.

En la barra superior eliges el **origen** y pulsas *Conectar por servidor local*:

| Origen | Para qué sirve | Necesitas |
|---|---|---|
| **Arduino real (COM3)** | tu placa por USB | el servidor tiene el COM3 abierto |
| **Simulador Wokwi** | el simulador de VS Code | la simulación debe estar corriendo |
| **Conectar por USB (Web Serial)** | Web Serial directo | solo Chrome/Edge, y el COM3 libre |
| **Modo demo** | sin nada de hardware | nada |

> **El COM3 es exclusivo: solo un programa puede tenerlo abierto.** Si la
> extensión de Wokwi de VS Code o el propio dashboard con Web Serial lo tienen,
> el servidor responde `503`. Ciérralos y dale a *Conectar por servidor local*.

### El puente del servidor

`serve.ps1` traduce a HTTP:

| Endpoint | Qué hace |
|---|---|
| `GET  /api/health?src=com\|wokwi` | si el origen está conectado |
| `GET  /api/state?src=com\|wokwi` | último estado JSON recibido |
| `GET  /api/raw?src=com` | log crudo de la placa (incluye los `[TEST]`) |
| `POST /api/send?src=com\|wokwi` | reenvía un comando JSON |
| `POST /api/open?src=com` | reintenta abrir el COM3 |

* Contra el **simulador** usa el puerto RFC2217 (`rfc2217ServerPort = 4000`).
* Contra la **placa real** usa el puerto serie COM3.
* Todo se maneja **en memoria**, en un solo proceso: la latencia entre el clic
  y la respuesta de la placa es de **30-130 ms**.

---

## 6. Simular en Wokwi

```powershell
.\build.ps1
```

Luego en VS Code: `F1` → **Wokwi: Start Simulation**.

| Control | Efecto |
|---|---|
| Botón rojo del sensor (o tecla `1`) | interrumpe el sensor → la plaza 1 sale OCUPADA |
| Dashboard → "Auto en plaza 2" | simula un auto en la plaza 2 |
| Dashboard → "Abrir/Cerrar barrera" | mueve el servo |
| Dashboard → "Cambiar LCD" | cambia el modo de la pantalla |

> **Wokwi no emula el UNO R4 WiFi** (es un ARM Cortex-M4); solo emula micros AVR
> (ATmega328P). Por eso en Wokwi se compila con `--fqbn arduino:avr:uno` y en la
> placa real con `--fqbn arduino:renesas_uno:unor4wifi`. El código usa solo
> pines compatibles entre ambas placas.

---

## 7. Compilar y subir

```powershell
.\build.ps1 -Target both        # compila para Wokwi (AVR) y para el R4
.\build.ps1 -Target r4 -Flash   # compila y sube por USB a la placa
```

También a mano:

```powershell
arduino-cli compile --fqbn arduino:renesas_uno:unor4wifi --output-dir parknet\build-r4 parknet
arduino-cli upload  --fqbn arduino:renesas_uno:unor4wifi --port COM3 --input-dir parknet\build-r4
```

El mismo sketch funciona en el **UNO clásico** (ATmega328P) sin tocar nada:
solo cambia la placa que eliges en el IDE.
*(Ojo: el AVR tiene 2 KB de RAM y el sketch va al 94%. Si añades cosas, quita
textos o pasa las cadenas a `F()`.)*

---

## 8. Diagnóstico

```powershell
wokwi-cli lint .                                # valida el diagrama
.\check-cables.ps1                              # comprueba que no hay cruces
.\draw-diagram.ps1                              # esquema a PNG
arduino-cli core list                           # cores instalados
Invoke-WebRequest http://localhost:8765/api/health
Invoke-WebRequest http://localhost:8765/api/raw?src=com   # log crudo de la placa
```

Si editas `diagram.json` y el simulador sigue mostrando lo anterior, recarga
la ventana: `Ctrl+Shift+P` → **Developer: Reload Window** (y si pide guardar
el diagrama, **no** guardes: el fichero del disco ya está actualizado).

### Problemas frecuentes

| Síntoma | Causa probable |
|---|---|
| La plaza 1 sale siempre OCUPADA | el sensor no está bien: comprueba **GND/VCC/DO**, que el DO esté en un pin de **entrada** (13) y gira el potenciómetro del módulo |
| Al mover el servo se apagan los LEDs | el servo está cogiendo el 5 V del Arduino → **5 V externo + masa común** |
| El módulo del sensor se calienta | VCC y GND cambiados, o el DO en un pin de salida |
| La LCD no enciende | probar `LCD_ADDR 0x3F`, o revisar los jumpers A0/A1 del backpack |
| El LED no cambia de estado | el filtro antirrebote (`FILTRO_LECTURAS`) o el pin equivocado |

---

## 9. Controlar el parking desde el móvil (modo WiFi)

El **UNO R4 WiFi** puede servir **su propio panel web**, así que el móvil entra
**directo al Arduino** y el PC no hace falta para nada (el Arduino queda
autónomo, alimentado con un cargador de móvil).

### 1. Rellenar la red (2 líneas)

En `parknet/parknet.ino`:

```cpp
#define WIFI_SSID   "HONOR ola"                      // el nombre de tu red
#define WIFI_PASS   "AQUI_PON_TU_CLAVE_DEL_HOTSPOT" // la clave
```

> No hace falta que la red tenga **internet**: el hotspot de un móvil es
> simplemente una red WiFi a la que se pueden conectar varios dispositivos.

### 2. Subir el sketch

```powershell
.\build.ps1 -Target r4 -Flash
```

### 3. Leer la IP del Arduino

En el Monitor Serie (115200) aparece:

```
[WIFI] IP del Arduino: 192.168.43.123
[WIFI] abre esa IP en el navegador del movil
```

### 4. Entrar desde el móvil

Abre `http://192.168.43.123` (la IP que salió arriba) en el navegador del
teléfono. El panel sirve el **nº de plazas libres**, el estado de cada plaza y
los botones de la barrera, el auto simulado y el modo de la LCD.

Rutas que sirve el propio Arduino:

| Ruta | Qué hace |
|---|---|
| `GET /` | el panel web |
| `GET /api/state` | el estado en JSON |
| `POST /api/send` | un comando JSON (los mismos que por USB) |

### 5. Que quede autónomo de verdad

- Alimenta el R4 con un **cargador de móvil (5 V / 1 A)** (el WiFi + el servo
  piden más que el USB del PC).
- Si el hotspot del móvil **bloquea el tráfico entre dispositivos**
  (algunos lo hacen), cambia a `WIFI_MODO_AP 1`: el Arduino crea su propia red
  **"ParkNet"** y el móvil se conecta a ella (clave `parknet2026`) → panel en
  `http://192.168.4.1`.

> El sketch sigue funcionando **por USB** a la vez: el R4 tiene USB y WiFi, así
> que puedes ver el Monitor Serie en el PC y controlar el panel desde el móvil.

---

## 10. MQTT: responder al sistema del grupo (Mosquitto)

El sketch de ParkNet incluye `parknet_mqtt.h`: el hardware de verdad
(barrera/servo, LCD, LEDs) **responde a los comandos MQTT del broker
Mosquitto del grupo** y publica el estado del parking.

```
hardware ParkNet  <->  WiFi (2.4 GHz)  <->  Mosquitto en el PC  <->  comandos
```

**Topics (los del sistema del grupo; cambia "grupo1" si la clase asigna otro):**

| Topic | Sentido | Contenido |
|---|---|---|
| `iot/grupo1/servo` | PC → Arduino | `ABRIR`, `CERRAR` o un ángulo 0-180 |
| `iot/grupo1/estado` | Arduino → PC | `{"libres":1,"total":2,"ocupadas":[1,0],"barrera":"ABIERTA","lcd":"datos"}` |
| `iot/grupo1/plazas` | Arduino → PC | número de plazas libres |

**Configuración (una vez):**

1. En `parknet/parknet_mqtt.h`, cambia `mqttServer` por la **IPv4 del PC que
   corre Mosquitto** (se ve con `ipconfig`). Revisa la IP cada vez que
   cambiéis de red.
2. El broker se monta igual que en la guía del grupo (Mosquitto,
   `listener 1883` + `allow_anonymous true`, firewall, `-v` como evidencia).
3. Súbelo a la placa y conecta PC y Arduino a la **misma red 2.4 GHz** (o al
   hotspot del móvil).

**Probar:** `mosquitto_pub -h localhost -t "iot/grupo1/servo" -m "ABRIR"` →
el servo de la barrera se mueve de verdad, la LCD se actualiza y el estado
vuelve por `iot/grupo1/estado`.

Mientras no haya broker reachable, ParkNet sigue operando solo:
el panel por su WiFi, el sensor IR y los LEDs no dependen de MQTT.

## 11. Origen del proyecto

Adaptado de la sección **Parking** del repositorio
[`BenedettoSimone/SmartCity-Arduino`](https://github.com/BenedettoSimone/SmartCity-Arduino)
(6 plazas con IR + matriz 8x8 MAX7219 + barrera). De ahí viene el **layout de
la matriz** (bloques de 3×2 LEDs con pasillo). Lo que **no** se ha trayado:
Node-RED (`flows.json`), las variantes MQTT, los modelos 3D y los otros tres
proyectos (calidad del aire, inundaciones y farolas).

Aquí **no** se usa la matriz 8×8: la pantalla es un **LCD 1602 con conversor
I2C**, hay LEDs por plaza, la barrera se maneja desde la web y el dashboard es
HTML propio (sin Node-RED ni internet).

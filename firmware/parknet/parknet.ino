// =====================================================
// ParkNet 3.0 - Estacionamiento Inteligente (2 plazas)
// Equipo Fixmol - Internet de las Cosas
// =====================================================
// SISTEMA:
//   PLAZA 1 -> sensor IR (KY-033) + LED propio. Si algo interrumpe el haz,
//              el sensor la marca y la plaza sale OCUPADA.
//   PLAZA 2 -> plaza virtual: se simula desde la pagina del PC.
//   LED por plaza      -> ENCENDIDO = plaza LIBRE
//   Pantalla LCD 1602  -> 4 lineas de informacion (2 visibles + rotacion)
//                         conmutable entre DATOS y el nombre FIXMOL
//   Barrera (servo)    -> se abre y se cierra desde la pagina del PC
//   Link al PC         -> JSON bidireccional por USB
//
// HARDWARE (Arduino UNO R4 WiFi, tambien vale el UNO clasico):
//   pin 13  sensor IR plaza 1              (LOW = hay obstaculo = ocupada)
//   pin 12  LED plaza 1                    (encendido = libre)
//   pin 10  LED plaza 2                    (encendido = libre, virtual)
//   pin A0  servo de la barrera            (90 grados abierta / 0 cerrada)
//   pin A4  LCD 1602 SDA  (conversor PCF8574)
//   pin A5  LCD 1602 SCL
//   +5V / GND del LCD al rail de la protoboard
//
// PROTOCOLO (lineas JSON, 115200 baud):
//   Arduino -> PC :
//     {"t":"state","ocupadas":[0,0],"libres":2,"total":2,
//      "barrera":"CERRADA","auto":null,"lcd":"datos"}
//     ocupadas[i] = 1 si la plaza i esta OCUPADA, 0 si esta LIBRE
//     barrera     = "ABIERTA" | "CERRADA"
//     auto        = "in" | "out" | null (ultimo cambio en la plaza 2)
//     lcd         = "datos" | "fixmol" (lo que muestra la pantalla)
//
//   PC -> Arduino :
//     {"cmd":"toggle","slot":2}          simula un auto en la plaza 2
//     {"cmd":"barrera","estado":"ABIERTA"}  abre la barrera con el servo
//     {"cmd":"barrera","estado":"CERRADA"}  cierra la barrera
//     {"cmd":"lcd","modo":"fixmol"}       cambia la pantalla a FIXMOL
//     {"cmd":"lcd","modo":"datos"}        vuelve a los datos
//     {"cmd":"sensores"}                 deja la plaza 2 libre
//     {"cmd":"sync"}                     reenvia el estado
//
// En Wokwi el UNO R4 se simula compilando para arduino:avr:uno.
// =====================================================

#include <Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// =====================================================
// MODO WIFI: el Arduino sirve su PROPIO panel web en el WiFi.
// Asi el movil entra DIRECTO al Arduino (el PC no hace falta para nada).
//   - Rellena el SSID y la clave de tu red (el hotspot del movil vale: solo
//     necesita ser una red WiFi, no hace falta que haya internet).
//   - El Arduino se alimenta con un cargador de movil (5 V / 1 A), asi queda
//     totalmente autonomo.
//   - Al arrancar, el monitor serie imprime la IP: esa es la que abres en
//     el navegador del movil.
// =====================================================
#define USAR_WIFI   1
#define WIFI_SSID   "HONOR ola"
#define WIFI_PASS   "quesesoo"
#define WIFI_PUERTO 80
// 0 = el R4 se une a tu red (hotspot del movil). El movil sigue en la misma
//     red y abre la IP del Arduino.  [necesita la clave correcta]
// 1 = el R4 crea su propia red "ParkNet" y el movil se conecta a ella.
//     No necesita clave de ninguna red y funciona siempre.  [RECOMENDADO]
#define WIFI_MODO_AP 0
#define WIFI_AP_SSID "ParkNet"
#define WIFI_AP_PASS "parknet2026"

// =====================================================
// Pantalla LCD 1602 con conversor I2C (PCF8574).
// Se usa la libreria estandar LiquidCrystal_I2C, que es la que recomienda
// la documentacion de Wokwi para esta pieza y la que entiende el
// simulador. No se usa su frame buffer: la memoria del UNO clasico
// (2 KB) es muy escasa y con los textos en flash (F()) cabe de sobra.
// El mapeo de pines del PCF8574 es el estandar de los backpack habituales:
//   P0=RS  P1=RW  P2=EN  P3=retroiluminacion  P4..P7=D4..D7
// =====================================================
// Direcciones habituales del conversor: 0x27 (la de la mayoria) o 0x3F.
#define LCD_ADDR   0x27
#define LCD_COLS   16
#define LCD_LINEAS 4      // lineas de contenido (el 1602 muestra 2 a la vez)
#define LCD_ROT_MS 4000   // cada cuanto rota el detalle de la 2a linea (ms)

// --- CONFIGURACION ---
#define NUM_SLOTS      2
#define MAX_SLOTS      2
#define SERIAL_BAUD 115200

// Slot 1 = sensor fisico | Slot 2 = plaza virtual (la controla el PC)
#define SLOT_SENSOR 0
#define SLOT_VIRTUAL 1

// --- PINES ---
//   plaza 1: sensor pin 13 | LED pin 12
//   plaza 2: sin sensor ni LED (se simula desde la web)
#define IR_SLOT_1   13
#define SERVO_PIN   A0
#define LCD_SDA     A4
#define LCD_SCL     A5

// --- UMBRALES ---
#define BARRERA_ANGULO_ABIERTA  90
#define BARRERA_ANGULO_CERRADA 0

// --- Polaridad del sensor IR ---
// El KY-033 es ACTIVO EN LOW: LOW = ve algo reflectante = plaza ocupada.
// Ponlo a 1 solo si tu modulo va al reves (HIGH = obstaculo).
#define SENSOR_INVERTIDO 0

// --- Filtro antirrebote ---
// El sensor puede dar lecturas ruidosas. El estado solo cambia tras
// FILTRO_LECTURAS lecturas iguales seguidas, asi el LED no parpadea.
#define FILTRO_LECTURAS 3

// --- Proteccion frente a caidas de tension ---
// El servo pide 100-200 mA al moverse. Si se alimenta del pin 5V del
// Arduino el rail se hunde: el micro se reinicia por brownout (se apagan
// los LEDs) y los sensores dan lecturas falsas. Mientras la tension se
// recupera no se leen los slots con sensor: se mantiene el ultimo estado.
#define RECUPERACION_SERVO_MS 600
unsigned long servoSeMovio = 0;

#define TIEMPO_PARKING 4000

// --- ARRAYS ---
// int8_t (1 byte) en vez de int (2 bytes) para ahorrar RAM en el UNO
int8_t pinSensor[MAX_SLOTS] = { IR_SLOT_1, -1 };
int8_t pinLed[MAX_SLOTS]    = { 12, 10 };

// --- OBJETOS ---
Servo    barrera;
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, 2);

// --- ESTADO ---
bool slotOccupied[MAX_SLOTS] = { false, false };

// -1 = lo decide el sensor real | 0 = libre forzado | 1 = ocupada forzada
int slotOvr[MAX_SLOTS] = { -1, -1 };

bool barreraAbierta = false;

// Filtro antirrebote del sensor: valor confirmado y lecturas que lo confirman.
int8_t  filtroValor[MAX_SLOTS] = { 0, 0 };
uint8_t filtroVeces[MAX_SLOTS] = { 0, 0 };

// Pantalla
uint8_t lcdLinea = 0;        // linea visible (0 o 2)
uint8_t lcdModo  = 0;        // 0 = datos, 1 = fixmol
unsigned long lcdTic = 0;

// Durante los tests el bucle no debe pisar las salidas
bool testEnCurso = false;

// --- LINK AL PC ---
char linea[64];     // el comando mas largo: {"cmd":"barrera","estado":"CERRADA"}
int  lineaLen = 0;

// =====================================================
// Funciones de estado
// =====================================================

int plazasLibres() {
  int libres = 0;
  for (int i = 0; i < NUM_SLOTS; i++) {
    if (!slotOccupied[i]) libres++;
  }
  return libres;
}

// Lee la plaza: si el PC la tiene forzada manda el PC,
// si no, manda el sensor IR (LOW = sensor interrumpido = auto presente).
bool leerSlot(int i) {
  if (slotOvr[i] >= 0) return slotOvr[i] == 1;
  if (pinSensor[i] < 0) return false;
  int v = digitalRead(pinSensor[i]);
  if (SENSOR_INVERTIDO) v = !v;
  return v == LOW;          // KY-033: LOW = hay algo delante
}

// LED por plaza: ENCENDIDO = LIBRE
void actualizarLeds() {
  for (int i = 0; i < NUM_SLOTS; i++) {
    if (pinLed[i] >= 0) {
      digitalWrite(pinLed[i], slotOccupied[i] ? LOW : HIGH);
    }
  }
}

// =====================================================
// Pantalla LCD 1602 (4 lineas, se ven 2 y van rotando)
// =====================================================

// Escribe una linea de 16 caracteres (recorta o rellena con espacios)
void lcdPonerLinea(uint8_t fila, const char* txt) {
  char buf[LCD_COLS + 1];
  snprintf(buf, sizeof(buf), "%-*.*s", LCD_COLS, LCD_COLS, txt);
  lcd.setCursor(0, fila);
  lcd.print(buf);
}

// Contenido de la pantalla segun el modo.
//   MODO DATOS : linea 1 = cuantos espacios libres (SIEMPRE visible y al
//                 instante), linea 2 = el detalle que va rotando.
//   MODO FIXMOL: linea 1 = el nombre del equipo, linea 2 = texto del equipo.
void lcdPintar() {
  char buf[LCD_COLS + 1];
  if (lcdModo == 1) {
    // ---- modo FIXMOL: solo el nombre del equipo ----
    lcdPonerLinea(0, "*** FIXMOL ***");
    const char* extra;
    switch (lcdLinea % 3) {
      case 0:  extra = "ESTACIONAMIENTO"; break;
      case 1:  extra = "INTELIGENTE IoT";  break;
      default: extra = "  2 PLAZAS  ";       break;
    }
    lcdPonerLinea(1, extra);
  } else {
    // ---- modo DATOS: cuantos espacios hay libres ----
    snprintf(buf, sizeof(buf), "LIBRES: %d/%d", plazasLibres(), NUM_SLOTS);
    lcdPonerLinea(0, buf);
    // la segunda linea va rotando con el detalle
    if (lcdLinea == 0) {
      snprintf(buf, sizeof(buf), "P1: %s", slotOccupied[0] ? "OCUPADA" : "LIBRE");
    } else if (lcdLinea == 1) {
      snprintf(buf, sizeof(buf), "P2: %s", slotOccupied[1] ? "OCUPADA" : "LIBRE");
    } else {
      snprintf(buf, sizeof(buf), "BARRERA: %s", barreraAbierta ? "ABIERTA" : "CERRADA");
    }
    lcdPonerLinea(1, buf);
  }
}

// Rota el detalle de la segunda linea cada LCD_ROT_MS
void lcdActualizar() {
  unsigned long ahora = millis();
  if (ahora - lcdTic >= LCD_ROT_MS) {
    lcdTic = ahora;
    lcdLinea = (lcdLinea + 1) % 3;
    lcdPintar();
  }
}

// =====================================================
// Barrera
// =====================================================

void abrirBarrera() {
  barrera.write(BARRERA_ANGULO_ABIERTA);
  barreraAbierta = true;
  servoSeMovio = millis();   // el rail se hunde al mover: esperamos a que se recupere
}

void cerrarBarrera() {
  barrera.write(BARRERA_ANGULO_CERRADA);
  barreraAbierta = false;
  servoSeMovio = millis();   // el rail se hunde al mover: esperamos a que se recupere
}

// =====================================================
// Link con el PC (JSON)
// =====================================================

void enviarEstado(const char* autoEvt) {
  char buf[160];
  int n = snprintf(buf, sizeof(buf), "{\"t\":\"state\",\"ocupadas\":[");
  for (int i = 0; i < NUM_SLOTS; i++) {
    n += snprintf(buf + n, sizeof(buf) - n, "%s%d", i ? "," : "", slotOccupied[i] ? 1 : 0);
  }
  n += snprintf(buf + n, sizeof(buf) - n,
                "],\"libres\":%d,\"total\":%d,\"barrera\":\"%s\",\"auto\":",
                plazasLibres(), NUM_SLOTS,
                barreraAbierta ? "ABIERTA" : "CERRADA");
  if (autoEvt) {
    n += snprintf(buf + n, sizeof(buf) - n, "\"%s\"", autoEvt);
  } else {
    n += snprintf(buf + n, sizeof(buf) - n, "null");
  }
  n += snprintf(buf + n, sizeof(buf) - n, ",\"lcd\":\"%s\"}", lcdModo ? "fixmol" : "datos");
  Serial.println(buf);
}

// Banner legible para el Serial Monitor
void imprimirEstadoHumano() {
  Serial.print(F("  LIBRES: "));
  Serial.print(plazasLibres());
  Serial.print(F(" / "));
  Serial.println(NUM_SLOTS);
  for (int i = 0; i < NUM_SLOTS; i++) {
    Serial.print(F("  Plaza "));
    Serial.print(i + 1);
    Serial.print(F(" ("));
    Serial.print(pinSensor[i] >= 0 ? "sensor IR" : "web");
    Serial.print(F("): "));
    Serial.println(slotOccupied[i] ? "OCUPADA" : "LIBRE");
  }
  Serial.print(F("  Barrera: "));
  Serial.println(barreraAbierta ? "ABIERTA" : "CERRADA");
  Serial.print(F("  Pantalla: "));
  Serial.println(lcdModo ? "FIXMOL" : "DATOS");
}

// =====================================================
// Comandos que llegan del PC
// =====================================================

int jsonInt(const char* s, const char* key) {
  char pat[24];
  snprintf(pat, sizeof(pat), "\"%s\"", key);
  const char* p = strstr(s, pat);
  if (!p) return -1;
  p = strchr(p + strlen(pat), ':');
  if (!p) return -1;
  return atoi(p + 1);
}

void ejecutarComando(const char* cmd) {

  // --- Simular un auto en cualquier plaza (la web fuerza al sensor) ---
  if (strstr(cmd, "\"toggle\"")) {
    int s = jsonInt(cmd, "slot") - 1;
    if (s < 0 || s >= NUM_SLOTS) return;

    slotOvr[s] = slotOvr[s] == 1 ? 0 : 1;
    slotOccupied[s] = (slotOvr[s] == 1);

    Serial.print(F("[PC] Plaza "));
    Serial.print(s + 1);
    Serial.println(slotOccupied[s] ? " -> OCUPADA (hay auto)" : " -> LIBRE");

    imprimirEstadoHumano();
    actualizarLeds();
    lcdPintar();
    enviarEstado(slotOccupied[s] ? "in" : "out");
    return;
  }

  // --- Abrir / cerrar la barrera con el servo ---
  if (strstr(cmd, "\"barrera\"")) {
    if (strstr(cmd, "\"ABIERTA\"")) {
      abrirBarrera();
      Serial.println(F("[PC] Barrera ABIERTA"));
    } else {
      cerrarBarrera();
      Serial.println(F("[PC] Barrera CERRADA"));
    }
    Serial.print(F("  -> servo comanda "));
    Serial.print(barrera.read());
    Serial.print(F(" grados ("));
    Serial.print(barrera.readMicroseconds());
    Serial.println(F(" us)"));
    imprimirEstadoHumano();
    lcdPintar();
    enviarEstado(NULL);
    return;
  }

  // --- Cambiar el modo de la pantalla LCD ---
  if (strstr(cmd, "\"lcd\"")) {
    if (strstr(cmd, "\"fixmol\""))      lcdModo = 1;
    else if (strstr(cmd, "\"datos\""))  lcdModo = 0;
    lcdLinea = 0;
    lcdTic = millis();
    lcd.clear();
    lcdPintar();
    Serial.print(F("[PC] Pantalla LCD en modo "));
    Serial.println(lcdModo ? "FIXMOL" : "DATOS");
    enviarEstado(NULL);
    return;
  }

  // --- Soltar los overrides: vuelven a mandar los sensores ---
  if (strstr(cmd, "\"sensores\"")) {
    for (int i = 0; i < NUM_SLOTS; i++) { slotOvr[i] = -1; }
    Serial.println(F("[PC] Vuelven a mandar los sensores"));
    imprimirEstadoHumano();
    actualizarLeds();
    lcdPintar();
    enviarEstado(NULL);
    return;
  }

  // --- Escanear el bus I2C: sirve para saber si la LCD responde ---
  if (strstr(cmd, "\"i2c\"")) {
    // 1) Estado de las dos lineas del bus (con las resistencias de pull-up)
    pinMode(A4, INPUT);
    pinMode(A5, INPUT);
    delay(5);
    int sda = digitalRead(A4);
    int scl = digitalRead(A5);
    Serial.print(F("[I2C] Linea SDA (A4) = "));
    Serial.print(sda == HIGH ? F("HIGH (libre)") : F("LOW  (CORTOCIRCUITO a GND)"));
    Serial.print(F("   |   SCL (A5) = "));
    Serial.print(scl == HIGH ? F("HIGH (libre)") : F("LOW  (CORTOCIRCUITO a GND)"));
    Serial.println();
    pinMode(A4, INPUT_PULLUP);
    pinMode(A5, INPUT_PULLUP);

    // 2) Barrido de direcciones (ACK) y lectura de las dos direcciones tipicas
    Serial.println(F("[I2C] Escaneando el bus (A4=SDA, A5=SCL)..."));
    uint8_t encontrados = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
      Wire.beginTransmission(addr);
      if (Wire.endTransmission() == 0) {
        Serial.print(F("[I2C]   ACK en 0x"));
        if (addr < 16) Serial.print('0');
        Serial.print(addr, HEX);
        Serial.println();
        encontrados++;
      }
    }
    if (encontrados == 0) {
      // Ningun ACK: probamos a LEER de las dos direcciones tipicas
      Serial.println(F("[I2C] Sin ACK en ninguna direccion. Probando a leer:"));
      for (uint8_t i = 0; i < 2; i++) {
        uint8_t addr = (i == 0) ? 0x27 : 0x3F;
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
          uint8_t n = Wire.requestFrom(addr, (uint8_t)1);
          Serial.print(F("[I2C]   0x"));
          Serial.print(addr, HEX);
          Serial.print(F(" responde, lectura = "));
          Serial.print(n);
          Serial.print(F(" byte(s): "));
          for (uint8_t k = 0; k < n; k++) Serial.print(Wire.read(), HEX);
          Serial.println();
        } else {
          Serial.print(F("[I2C]   0x"));
          Serial.print(addr, HEX);
          Serial.println(F(" no responde"));
        }
      }
    }
    if (encontrados == 0) {
      Serial.println(F("[I2C] NADIE responde en el bus."));
      Serial.println(F("[I2C] Revisa: VCC a +5V, GND comun, SDA en A4, SCL en A5,"));
      Serial.println(F("[I2C] y el jumper de contraste (el potenciometro azul)."));
    } else if (encontrados > 3) {
      Serial.println(F("[I2C] OJO: responden casi todas las direcciones =>"));
      Serial.println(F("[I2C] el bus esta atascado: SDA o SCL cortocircuitados a GND."));
    } else {
      Serial.println(F("[I2C] fin del escaneo."));
    }
    return;
  }

  if (strstr(cmd, "\"sync\"")) {
    enviarEstado(NULL);
    return;
  }

  // --- Test de servo bajo demanda ---
  if (strstr(cmd, "\"test_servo\"")) {
    testServo();
    enviarEstado(NULL);
    return;
  }

  // --- Test de cableado ---
  if (strstr(cmd, "\"test_cables\"")) {
    testCables();
    return;
  }
}

void procesarComandos() {
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '\n') {
      linea[lineaLen] = '\0';
      if (lineaLen > 0) ejecutarComando(linea);
      lineaLen = 0;
    } else if (c != '\r' && lineaLen < (int)sizeof(linea) - 1) {
      linea[lineaLen++] = c;
    }
  }
}

// =====================================================
// Autodiagnostico del servo
// =====================================================
// Mide el duty cycle REAL del pin de senal leyendo el propio pin.
// Periodo del servo = 20000 us
//   0   grados ->  544 us -> ~2.7 % en alto
//   90  grados -> 1472 us -> ~7.4 % en alto
//   180 grados -> 2400 us -> ~12.0 % en alto
// IMPORTANTE: se usa aritmetica ENTERA a proposito. Si se usara float,
// Serial.print(float) arrastraria el printf de punto flotante y el UNO
// (2 KB de RAM) se quedaria sin memoria.
uint16_t medirDutyCycle(uint8_t pin, uint16_t ms) {
  uint32_t altos = 0, total = 0;
  unsigned long t0 = millis();
  while ((millis() - t0) < ms) {
    altos += digitalRead(pin);
    total++;
  }
  return total ? (uint16_t)((1000UL * altos) / total) : 0;
}

void imprimirDuty(const char* etiq, uint16_t d10) {
  Serial.print(F("  [TEST] "));
  Serial.print(etiq);
  Serial.print(F(" -> "));
  Serial.print(d10 / 10);
  Serial.print('.');
  Serial.print(d10 % 10);
  Serial.println(F(" % en alto"));
}

// Barrido de prueba: abre a 90 grados y vuelve a 0, midiendo la senal.
void testServo() {
  Serial.println(F("  [TEST] Probando el servo..."));
  barrera.write(BARRERA_ANGULO_CERRADA);
  delay(600);
  uint16_t d0 = medirDutyCycle(SERVO_PIN, 300);
  imprimirDuty("0 grados ", d0);

  barrera.write(BARRERA_ANGULO_ABIERTA);
  delay(900);
  uint16_t d90 = medirDutyCycle(SERVO_PIN, 300);
  imprimirDuty("90 grados", d90);

  if (d90 < 10) {
    Serial.println(F("  [TEST] AVISO: el pin NO genera senal de servo."));
    Serial.println(F("  [TEST] Revisa la libreria o el pin; el cableado no es el problema."));
  } else {
    Serial.println(F("  [TEST] OK: el pin genera senal de servo correctamente."));
  }
  barrera.write(BARRERA_ANGULO_CERRADA);
  delay(400);
}

// =====================================================
// Test de cableado
// =====================================================
// Enciende y apaga cada salida por separado para que se pueda comprobar
// que lo que hay enchufado en cada pin es lo que crees.
void testCables() {
  testEnCurso = true;

  // --- Estado CRUDO de los sensores (lo primero que hay que mirar) ---
  // El KY-033 es ACTIVO EN LOW: DO = LOW si tiene algo reflectante delante
  // (una tarjeta blanca, la mano muy cerca) y HIGH con el paso libre.
  // Si marca LOW siempre: revisa VCC/GND y que el DO NO este en un pin de
  // salida, o gira el potenciometro del modulo.
  // OJO: en AVR los textos literales ocupan RAM, hay que meterlos en flash
  // con F() o el UNO clasico se queda sin memoria.
  Serial.println(F("[CABLES] Sensores (LOW = ve algo, HIGH = libre):"));
  for (int i = 0; i < NUM_SLOTS; i++) {
    if (pinSensor[i] < 0) continue;
    int v = digitalRead(pinSensor[i]);
    Serial.print(F("[CABLES]   plaza "));
    Serial.print(i + 1);
    Serial.print(F("  pin "));
    Serial.print(pinSensor[i]);
    Serial.print(F("  ->  "));
    Serial.println(v == LOW ? F("LOW  (ve algo)") : F("HIGH (libre)"));
  }
  Serial.println(F("[CABLES] Si dan LOW siempre: revisa VCC/GND, que el DO este"));
  Serial.println(F("[CABLES] en un pin de ENTRADA (13) y gira el trimmer."));

  for (int n = 0; n < 2; n++) {
    Serial.print(F("[CABLES] LED pin "));
    Serial.print(pinLed[n]);
    Serial.println(F(" (3 parpadeos)..."));
    for (int i = 0; i < 3; i++) {
      digitalWrite(pinLed[n], HIGH); delay(600);
      digitalWrite(pinLed[n], LOW);  delay(600);
    }
  }

  Serial.println(F("[CABLES] SERVO pin A0: 0 -> 90 -> 180 -> 0"));
  for (int a = 0; a <= 180; a += 15) {
    barrera.write(a);
    Serial.print(F("[CABLES] servo = "));
    Serial.print(a);
    Serial.println(F(" grados"));
    delay(200);
  }
  barrera.write(BARRERA_ANGULO_CERRADA);
  delay(400);

  Serial.println(F("[CABLES] Test terminado"));
  testEnCurso = false;
  actualizarLeds();
  lcdPintar();
  enviarEstado(NULL);
}

// =====================================================
// SERVIDOR WEB EN EL ARDUINO (modo WiFi)
// Sirve su propio panel: el movil entra DIRECTO al Arduino, sin PC.
// Rutas:  GET  /            -> el panel
//         GET  /api/state   -> el estado en JSON
//         POST /api/send    -> un comando JSON (los mismos que por USB)
// =====================================================
#if USAR_WIFI && !defined(__AVR__)
  #include "parknet_wifi.h"
  #include "parknet_mqtt.h"     // <- broker Mosquitto del grupo (servo ABRIR/CERRAR)
#endif

// =====================================================
// SETUP
// =====================================================

void setup() {
  Serial.begin(SERIAL_BAUD);

  // En el UNO R4 el puerto USB puede tardar en abrirse.
  unsigned long t0 = millis();
  while (!Serial && (millis() - t0) < 2000) {
    delay(10);
  }

  for (int i = 0; i < NUM_SLOTS; i++) {
    if (pinSensor[i] >= 0) pinMode(pinSensor[i], INPUT_PULLUP);
    if (pinLed[i] >= 0) {
      pinMode(pinLed[i], OUTPUT);
      digitalWrite(pinLed[i], HIGH);   // arrancan todas libres
    }
  }

  // Pantalla LCD 1602 por I2C (A4 = SDA, A5 = SCL)
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcdTic = millis();

  barrera.attach(SERVO_PIN);
  barrera.write(BARRERA_ANGULO_CERRADA);
  Serial.print(F("  Servo attach(pin A0): "));
  Serial.println(barrera.attached() ? "OK" : "FALLO (sin timer PWM disponible)");
  Serial.print(F("  Servo angulo actual: "));
  Serial.println(barrera.read());
  testServo();

  Serial.println();
  Serial.println(F("  ParkNet 3.0 - Estacionamiento Inteligente"));
  Serial.println(F("  Equipo Fixmol"));
  Serial.println(F("  Iniciando..."));
  Serial.println();

  leerSlots();
  actualizarLeds();
  lcdPintar();
  imprimirEstadoHumano();
  Serial.println();
  Serial.println(F("  PLAZA 1: sensor IR (interrumpelo y sale OCUPADA) + LED pin 12"));
  Serial.println(F("  PLAZA 2: solo LED pin 10, se simula desde la pagina web"));
  Serial.println();
  Serial.println(F("  CABLEADO (con la placa desconectada):"));
  Serial.println(F("    plaza 1: sensor pin 13 | LED pin 12 (resistencia 330-470 ohm)"));
  Serial.println(F("    plaza 2: LED pin 10 (resistencia 330-470 ohm)"));
  Serial.println(F("    pin A0 -> SERVO senal (naranja) | rojo a +5V | marron a GND"));
  Serial.println(F("    pin A4 -> LCD SDA | pin A5 -> LCD SCL | LCD +5V y GND a los rails"));
  Serial.println();
  Serial.println(F("  LED ENCENDIDO = plaza LIBRE (pin en HIGH). APAGADO = plaza OCUPADA."));
  Serial.println();

#if USAR_WIFI && !defined(__AVR__)
  wifiConectar();
#if USAR_WIFI && !defined(__AVR__)
  mqttArrancar();        // <- broker del grupo: la barrera y el estado por MQTT
#endif
#endif

  enviarEstado(NULL);
}

// =====================================================
// LOOP
// =====================================================

void leerSlots() {
  bool railEstable = (millis() - servoSeMovio) >= RECUPERACION_SERVO_MS;
  bool cambio = false;                 // <- una plaza ha cambiado de estado
  for (int i = 0; i < NUM_SLOTS; i++) {
    // Slot con sensor fisico: si el servo se acaba de mover, el rail puede
    // estar hundido -> no se lee (se mantiene el ultimo estado valido).
    if (pinSensor[i] >= 0 && !railEstable) continue;

    bool v = leerSlot(i);
    if (v == filtroValor[i]) {
      filtroVeces[i] = 0;              // ya estaba confirmado
    } else {
      if (filtroVeces[i] < 250) filtroVeces[i]++;
      if (filtroVeces[i] >= FILTRO_LECTURAS) {
        filtroValor[i] = v;             // lectura confirmada -> cambia
        filtroVeces[i] = 0;
      }
    }
    if (slotOccupied[i] != filtroValor[i]) cambio = true;
    slotOccupied[i] = filtroValor[i];
  }
  // Si una plaza se ocupa o se libera, la pantalla se actualiza AL INSTANTE
  // (la cuenta de espacios libres tiene que cambiar en el momento).
  if (cambio) lcdPintar();
}

void loop() {
#if USAR_WIFI && !defined(__AVR__)
  wifiReintentar();       // <- se une a tu red en cuanto esta activa
  wifiServidor();        // <- el movil entra directo al Arduino
  mqttTic();             // <- el broker del grupo manda ABRIR/CERRAR y lee el estado
#endif
  procesarComandos();   // <- lo que manda el PC (plaza 2, barrera, pantalla)
  leerSlots();          // <- la plaza 1 la decide el sensor IR
  if (!testEnCurso) {
    actualizarLeds();
  }
  lcdActualizar();

  static unsigned long lastPrint = 0;
  if (millis() - lastPrint >= 1000) {
    lastPrint = millis();
    imprimirEstadoHumano();
    enviarEstado(NULL);
  }

  delay(50);
}

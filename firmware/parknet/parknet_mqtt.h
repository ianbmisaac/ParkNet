// =====================================================================
//  ParkNet <-> MQTT  v2  (autonomo: el Arduino habla SOLO con la pagina por Internet)
//
//  QUE ARREGLA (respecto a la v1):
//   1) La v1 publicaba SOLO en test.mosquitto.org, pero el panel admin escucha en
//      broker.hivemq.com y broker.emqx.io (y la pagina principal en mosquitto primero).
//      Sin el PC con el "puente" encendido, el admin nunca veia al Arduino.
//      Ahora se conecta a LOS TRES brokers a la vez y publica en todos.
//   2) La v1 no escuchaba "iot/fixmol/comando" (los botones del admin: barrera, simular
//      plazas, LCD). Eso solo llegaba por el cable USB (puente). Ahora el Arduino
//      atiende esos comandos por WiFi (con el token del grupo, igual que por USB).
//   3) El latido era cada 30 s y el admin marca "placa sin senal" a los 15 s. Ahora 8 s.
//   4) Una conexion fallida ya no deja el Arduino colgado 15 s: timeout corto, un solo
//      intento a la vez y reintentos espaciados.
//   5) El client-id ya no es fijo ("parknet-fixmol"): sale de la MAC de la placa, asi no
//      choca con otro dispositivo y el broker no te expulsa en bucle.
//   6) Mensajes de diagnostico claros en el Monitor Serie ([MQTT] ...).
//
//  Necesita la libreria PubSubClient 2.8 o mas nueva (Gestor de librerias).
//  La interfaz con el .ino es la misma de siempre: mqttArrancar() y mqttTic().
// =====================================================================

#ifndef PARKNET_MQTT_H
#define PARKNET_MQTT_H

#include <PubSubClient.h>

#define MQTT_NBROKERS 3
const char* const mqttHosts[MQTT_NBROKERS]   = { "test.mosquitto.org", "broker.hivemq.com", "broker.emqx.io" };
const char* const mqttNombres[MQTT_NBROKERS] = { "mosquitto", "hivemq", "emqx" };
const int         mqttPort = 1883;
// Si el Arduino solo tiene SU red propia "ParkNet" y un PC con Mosquitto se conecta a ella:
const char*       mqttServerAP = "192.168.4.2";

// Topics fijos de Fixmol (los mismos que usa la pagina)
const char* TOP_SERVO   = "iot/fixmol/servo";     // ABRIR / CERRAR / angulo
const char* TOP_COMANDO = "iot/fixmol/comando";   // JSON con token (admin)
const char* TOP_PLAZAS  = "iot/fixmol/plazas";    // plazas libres (numero)
const char* TOP_ESTADO  = "iot/fixmol/estado";    // estado completo (JSON)
const char* TOP_LOG     = "iot/fixmol/log";       // eventos ("SENSOR: PLAZA 1 OCUPADA")
// Los comandos del admin llevan  "k":"fixmol2026"  (igual que la pagina). Sin el, se ignoran.
const char* MQTT_TOKEN_PAT = "\"k\":\"fixmol2026\"";

// El sistema de la feria va por WiFi, sin puente USB. Si algun dia vuelves a
// usar el puente USB (mqtt-serie-bridge.js) AL MISMO TIEMPO que el WiFi, pon
// esto en 1 para que el Arduino ignore los "toggle" que lleguen por MQTT
// (los aplicaria el puente por USB) y no se ejecuten dos veces.
#define IGNORAR_TOGGLE_CON_USB 0

#define MQTT_LATIDO_MS     8000UL   // el admin avisa "sin senal" a los 15 s
#define MQTT_ENTRE_INTENTOS 3000UL  // minimo entre dos intentos de conexion (cualquier broker)

WiFiClient    mqWifi[MQTT_NBROKERS];
PubSubClient  mq[MQTT_NBROKERS];
bool          mqOk[MQTT_NBROKERS];
uint8_t       mqFallos[MQTT_NBROKERS];
unsigned long mqProx[MQTT_NBROKERS];

char          mqId[24] = "";
int           mqRonda = 0;
bool          mqApPrev = false;
unsigned long mqUltimoIntento = 0;
unsigned long mqProxLatido = 0;
unsigned long mqProxInforme = 0;

int           mqLibresAnt = -1;     // -1 = publicar ya
bool          mqBarrAnt = false;
bool          mqLcdAnt = false;
bool          mqSlotAnt[8];

uint32_t      mqUltHash = 0;        // anti-duplicados (el mismo mensaje puede llegar por 2-3 brokers)
unsigned long mqUltHashT = 0;

// ---------------------------------------------------------------- utilidades
int mqttConectados() {
  int n = 0;
  for (int i = 0; i < MQTT_NBROKERS; i++) if (mq[i].connected()) n++;
  return n;
}

const char* mqttHostDe(int i) {
  if (wifiAPActivo) return i == 0 ? mqttServerAP : NULL;   // en red propia: solo el PC de la red ParkNet
  return mqttHosts[i];
}

void mqttFijarId() {
  byte mac[6] = { 0, 0, 0, 0, 0, 0 };
  WiFi.macAddress(mac);
  if (mac[3] == 0 && mac[4] == 0 && mac[5] == 0) {          // la MAC aun no esta lista: algo unico igual
    unsigned long m = micros();
    mac[3] = (byte)(m >> 8); mac[4] = (byte)(m >> 16); mac[5] = (byte)(m >> 24);
  }
  snprintf(mqId, sizeof(mqId), "parknet-fx-%02X%02X%02X", mac[3], mac[4], mac[5]);
}

void mqttPublicarTodos(const char* topic, const char* texto) {
  for (int i = 0; i < MQTT_NBROKERS; i++)
    if (mq[i].connected()) mq[i].publish(topic, texto);
}

void mqttPublicarEstado() {
  if (mqttConectados() == 0) return;
  char buf[200];
  int n = snprintf(buf, sizeof(buf), "{\"libres\":%d,\"total\":%d,\"ocupadas\":[",
                   plazasLibres(), NUM_SLOTS);
  for (int i = 0; i < NUM_SLOTS && n > 0 && n < (int)sizeof(buf); i++) {
    n += snprintf(buf + n, sizeof(buf) - n, "%s%d", i ? "," : "", slotOccupied[i] ? 1 : 0);
  }
  if (n > 0 && n < (int)sizeof(buf)) {
    snprintf(buf + n, sizeof(buf) - n, "],\"barrera\":\"%s\",\"lcd\":\"%s\"}",
             barreraAbierta ? "ABIERTA" : "CERRADA",
             lcdModo ? "fixmol" : "datos");
  }
  mqttPublicarTodos(TOP_ESTADO, buf);
  char libresTxt[6];
  snprintf(libresTxt, sizeof(libresTxt), "%d", plazasLibres());
  mqttPublicarTodos(TOP_PLAZAS, libresTxt);
}

// ---------------------------------------------------------------- mensajes que llegan
void mqttCallback(char* topic, byte* payload, unsigned int len) {
  if (len == 0 || len > 100) return;
  char msg[104];
  memcpy(msg, payload, len);
  msg[len] = '\0';
  while (len > 0 && (msg[len - 1] == '\r' || msg[len - 1] == '\n' || msg[len - 1] == ' ')) msg[--len] = '\0';

  // el mismo comando puede llegar duplicado (por 2 o 3 brokers, o replicado por el puente del PC)
  uint32_t h = 5381;
  for (const char* p = topic; *p; p++) h = h * 33 + (uint8_t)*p;
  for (const char* p = msg; *p; p++) h = h * 33 + (uint8_t)*p;
  unsigned long ahora = millis();
  if (h == mqUltHash && ahora - mqUltHashT < 1200UL) return;
  mqUltHash = h;
  mqUltHashT = ahora;

  Serial.print(F("[MQTT] "));
  Serial.print(topic);
  Serial.print(F(" -> "));
  Serial.println(msg);

  if (strcmp(topic, TOP_COMANDO) == 0) {
    if (!strstr(msg, MQTT_TOKEN_PAT)) {
      Serial.println(F("[MQTT] comando SIN token valido: ignorado"));
      return;
    }
#if IGNORAR_TOGGLE_CON_USB
    // Con el cable USB abierto el puente del PC reenvia este mismo comando por
    // USB: "toggle" cambia de estado, asi que ejecutarlo dos veces lo anularia.
    if (strstr(msg, "\"toggle\"") && Serial) {
      Serial.println(F("[MQTT] toggle ignorado: USB conectado (lo atiende el puente USB)"));
      return;
    }
#endif
    ejecutarComando(msg);
    mqLibresAnt = -1;               // fuerza a publicar el estado nuevo ya
    return;
  }

  if (strcmp(topic, TOP_SERVO) == 0) {
    if (strcmp(msg, "ABRIR") == 0) {
      abrirBarrera();
    } else if (strcmp(msg, "CERRAR") == 0) {
      cerrarBarrera();
    } else {                        // un angulo: el servo va a esos grados
      int a = atoi(msg);
      if (msg[0] && a != 0) {
        a = constrain(a, 0, 180);
        barrera.write(a);
        barreraAbierta = (a != BARRERA_ANGULO_CERRADA);
      } else {
        Serial.println(F("[MQTT] comando no valido (ABRIR/CERRAR/angulo)"));
      }
    }
    imprimirEstadoHumano();
    lcdPintar();
    enviarEstado(NULL);
    mqLibresAnt = -1;
  }
}

// ---------------------------------------------------------------- conexion
void mqttDesconectarTodos() {
  for (int i = 0; i < MQTT_NBROKERS; i++) {
    if (mq[i].connected()) mq[i].disconnect();
    mqWifi[i].stop();
    mqOk[i] = false;
    mqProx[i] = 0;
    mqFallos[i] = 0;
  }
}

void mqttArrancar() {
  for (int i = 0; i < MQTT_NBROKERS; i++) {
    mq[i].setClient(mqWifi[i]);
    mq[i].setCallback(mqttCallback);
    mq[i].setKeepAlive(30);
    mq[i].setSocketTimeout(4);      // segundos: un broker caido no deja colgado al Arduino
    mqOk[i] = false;
    mqFallos[i] = 0;
    mqProx[i] = 0;
  }
  for (int i = 0; i < 8; i++) mqSlotAnt[i] = false;
  Serial.println(F("[MQTT] v2: me conecto a mosquitto + hivemq + emqx (como la pagina)"));
}

void mqttConectar(int i, const char* host) {
  if (mqId[0] == '\0') mqttFijarId();
  mqWifi[i].stop();
  mq[i].setServer(host, mqttPort);
  Serial.print(F("[MQTT] conectando a "));
  Serial.print(mqttNombres[i]);
  Serial.print(F(" ("));
  Serial.print(host);
  Serial.print(F(":"));
  Serial.print(mqttPort);
  Serial.print(F(") id "));
  Serial.println(mqId);
  if (mq[i].connect(mqId)) {
    mq[i].subscribe(TOP_SERVO);
    mq[i].subscribe(TOP_COMANDO);
    mqOk[i] = true;
    mqFallos[i] = 0;
    mqLibresAnt = -1;               // publica el estado enseguida
    Serial.print(F("[MQTT] OK "));
    Serial.print(mqttNombres[i]);
    Serial.println(F(": suscrito a servo y comando. La pagina ya me ve por ahi."));
  } else {
    if (mqFallos[i] < 250) mqFallos[i]++;
    unsigned long espera = 10000UL * mqFallos[i];
    if (espera > 60000UL) espera = 60000UL;
    mqProx[i] = millis() + espera;
    Serial.print(F("[MQTT] FALLO "));
    Serial.print(mqttNombres[i]);
    Serial.print(F(" estado="));
    Serial.print(mq[i].state());
    Serial.print(F("  (-2 = no llega al broker: revisa que el WiFi tenga Internet / puerto 1883). Reintento en "));
    Serial.print(espera / 1000UL);
    Serial.println(F(" s"));
  }
}

// Se llama en cada vuelta del loop. Casi nunca bloquea.
void mqttTic() {
  bool hayRed = (WiFi.status() == WL_CONNECTED) || wifiAPActivo;
  if (!hayRed) {
    if (mqttConectados() > 0 || mqOk[0] || mqOk[1] || mqOk[2]) {
      mqttDesconectarTodos();
      Serial.println(F("[MQTT] sin red WiFi, espero..."));
    }
    mqLibresAnt = -1;
    return;
  }
  if (wifiAPActivo != mqApPrev) {   // cambio de red (propia <-> tu WiFi): empezamos de cero
    mqApPrev = wifiAPActivo;
    mqttDesconectarTodos();
  }

  unsigned long ahora = millis();
  for (int i = 0; i < MQTT_NBROKERS; i++) {
    if (mq[i].connected()) {
      mq[i].loop();
    } else if (mqOk[i]) {
      mqOk[i] = false;
      mqProx[i] = ahora + 5000UL;
      Serial.print(F("[MQTT] se cayo la conexion con "));
      Serial.println(mqttNombres[i]);
    }
  }

  // un intento de conexion a la vez, rotando entre brokers
  if (ahora - mqUltimoIntento >= MQTT_ENTRE_INTENTOS) {
    for (int k = 0; k < MQTT_NBROKERS; k++) {
      int i = (mqRonda + k) % MQTT_NBROKERS;
      if (mq[i].connected()) continue;
      const char* host = mqttHostDe(i);
      if (host == NULL || ahora < mqProx[i]) continue;
      mqRonda = i + 1;
      mqttConectar(i, host);
      mqUltimoIntento = millis();
      break;
    }
  }

  int con = mqttConectados();
  if (con == 0) return;

  // publica al cambiar algo y como latido
  ahora = millis();
  bool cambio = (mqLibresAnt != plazasLibres()) || (mqBarrAnt != barreraAbierta) || (mqLcdAnt != lcdModo);
  for (int i = 0; i < NUM_SLOTS && i < 8; i++) {
    if (mqSlotAnt[i] != slotOccupied[i]) {
      cambio = true;
      if (i == 0 && mqLibresAnt != -1) {   // aviso de llegada para el admin
        mqttPublicarTodos(TOP_LOG, slotOccupied[0] ? "SENSOR: PLAZA 1 OCUPADA" : "SENSOR: PLAZA 1 LIBRE");
      }
    }
  }
  if (mqLibresAnt == -1 || cambio || ahora - mqProxLatido > MQTT_LATIDO_MS) {
    mqttPublicarEstado();
    mqProxLatido = ahora;
    mqLibresAnt = plazasLibres();
    mqBarrAnt = barreraAbierta;
    mqLcdAnt = lcdModo;
    for (int i = 0; i < NUM_SLOTS && i < 8; i++) mqSlotAnt[i] = slotOccupied[i];
  }

  // informe corto cada 30 s para el Monitor Serie
  if (ahora - mqProxInforme > 30000UL) {
    mqProxInforme = ahora;
    Serial.print(F("[MQTT] publicando en "));
    Serial.print(con);
    Serial.print(F("/3 brokers:"));
    for (int i = 0; i < MQTT_NBROKERS; i++) {
      Serial.print(F(" "));
      Serial.print(mqttNombres[i]);
      Serial.print(mq[i].connected() ? F("=OK") : F("=no"));
    }
    Serial.println();
  }
}

#endif  // PARKNET_MQTT_H

// =====================================================================
//  ParkNet <-> MQTT  (integracion con el sistema del grupo: Mosquitto)
//
//  ARQUITECTURA:
//    hardware ParkNet  <->  WiFi  <->  Mosquitto (PC)  <->  comandos
//
//  - El Arduino se SUSCRIBE a "iot/grupo1/servo":  ABRIR / CERRAR / un
//    angulo 0-180 mueven la barrera de verdad (servo en A0).
//  - El Arduino PUBLICA el estado real (plazas, barrera, pantalla) en
//    "iot/grupo1/estado" y el numero de plazas libres en
//    "iot/grupo1/plazas": al cambiar y como latido cada 30 s.
//  - Si no hay broker (o el PC no esta), ParkNet sigue funcionando igual:
//    USB, WiFi propia, sensor y LCD no dependen de esto.
//  - Los topics son los del sistema del amigo: cambiar "grupo1" por el
//    nombre que asigne la clase. Dos brokers posibles: mqttServerPC (la IP
//    del PC cuando la placa esta en la red del grupo) y mqttServerAP (el
//    PC conectado a la red propia "ParkNet" de la placa).
// =====================================================================

#ifndef PARKNET_MQTT_H
#define PARKNET_MQTT_H

#include <PubSubClient.h>

// ========================= CAMBIAR AQUI =============================
// IP del PC con Mosquitto (ipconfig -> IPv4). Se usa cuando el Arduino
// esta en la red del grupo (hotspot "HONOR ola" o la red del lab).
// NOTE 192.168.137.1 = hotspot de Windows del PROPIO PC (para probar en
// casa). El dia de la feria: la IP del PC del amigo que corra Mosquitto
// (se ve con ipconfig en ese PC).
const char* mqttServerPC = "192.168.137.1";
// Cuando el Arduino solo tiene SU red propia "ParkNet" y el PC (con el
// broker) se conecta a ella, el PC recibe 192.168.4.2 -> sin cambiar nada:
const char* mqttServerAP = "192.168.4.2";
const int   mqttPort     = 1883;
const char* mqttID       = "parknet-fixmol";   // unico por grupos (usar otro)
const char* TOP_SERVO  = "iot/grupo1/servo";   // <- comandos de barrera
const char* TOP_PLAZAS = "iot/grupo1/plazas";  // <- plazas libres
const char* TOP_ESTADO = "iot/grupo1/estado";  // <- estado completo
// ====================================================================

WiFiClient   mqttWifi;      // separado del del panel HTTP
PubSubClient mqtt(mqttWifi);

bool          mqttYaAviso    = false;   // para un solo mensaje de "OK"
unsigned long mqttProxIntento = 0;      // reintento de conexion (20 s)
unsigned long mqttProxLatido  = 0;      // publicacion periodica (30 s)
int           mqttLibresAnt  = -1;
bool          mqttBarrAnt    = false;
bool          mqttLcdAnt     = false;

void mqttPublicarEstado() {
  if (!mqtt.connected()) return;
  char buf[180];
  int n = snprintf(buf, sizeof(buf), "{\"libres\":%d,\"total\":%d,\"ocupadas\":[",
                   plazasLibres(), NUM_SLOTS);
  for (int i = 0; i < NUM_SLOTS && n > 0; i++) {
    n += snprintf(buf + n, sizeof(buf) - n, "%s%d", i ? "," : "",
                  slotOccupied[i] ? 1 : 0);
  }
  n += snprintf(buf + n, sizeof(buf) - n,
                "],\"barrera\":\"%s\",\"lcd\":\"%s\"}",
                barreraAbierta ? "ABIERTA" : "CERRADA",
                lcdModo ? "fixmol" : "datos");
  mqtt.publish(TOP_ESTADO, buf);
  char libresTxt[6];
  snprintf(libresTxt, sizeof(libresTxt), "%d", plazasLibres());
  mqtt.publish(TOP_PLAZAS, libresTxt);
}

void mqttCallback(char* topic, byte* payload, unsigned int len) {
  String msg;
  for (unsigned int i = 0; i < len; i++) msg += (char)payload[i];
  msg.trim();
  Serial.print(F("[MQTT] "));
  Serial.print(topic);
  Serial.print(F(" -> "));
  Serial.println(msg);

  if (String(topic) == TOP_SERVO) {
    if (msg == "ABRIR") {
      abrirBarrera();
    } else if (msg == "CERRAR") {
      cerrarBarrera();
    } else {                      // un angulo: el servo va a esos grados
      int a = constrain(msg.toInt(), 0, 180);
      if (msg.length() && msg.toInt() != 0) {
        barrera.write(a);
        barreraAbierta = (a != BARRERA_ANGULO_CERRADA);
        lcdPintar();
        enviarEstado(NULL);
      } else {
        Serial.println(F("[MQTT] comando no valido (ABRIR/CERRAR/angulo)"));
      }
    }
    imprimirEstadoHumano();
    lcdPintar();
    enviarEstado(NULL);
    mqttPublicarEstado();
  }
}

// El broker al que intentar conectar segun el modo de red actual
const char* mqttServerActual() {
  return wifiAPActivo ? mqttServerAP : mqttServerPC;
}

void mqttArrancar() {
  mqtt.setServer(mqttServerActual(), mqttPort);
  mqtt.setCallback(mqttCallback);
}

// Se llama en cada vuelta del loop. No bloquea.
void mqttTic() {
  // sin red del grupo Y sin red propia no hay broker a la vista
  if (WiFi.status() != WL_CONNECTED && !wifiAPActivo) {
    if (mqtt.connected()) mqtt.disconnect();
    if (mqttYaAviso) {
      mqttYaAviso = false;
      Serial.println(F("[MQTT] sin red del grupo, espero al broker..."));
    }
    mqttLibresAnt = -1;           // al volver, publica el estado fresco
    return;
  }

  mqtt.loop();                    // atiende al broker

  if (!mqtt.connected()) {
    if (mqttYaAviso) mqttYaAviso = false;
    unsigned long ahora = millis();
    if (ahora - mqttProxIntento < 20000) return;
    mqttProxIntento = ahora;
    Serial.print(F("[MQTT] conectando al broker "));
    Serial.print(mqttServerActual());
    Serial.print(F(":"));
    Serial.println(mqttPort);
    if (mqtt.connect(mqttID)) {
      mqtt.subscribe(TOP_SERVO);
      Serial.println(F("[MQTT] conectado y suscrito a "));
      Serial.println(TOP_SERVO);
      mqttYaAviso = true;
      mqttLibresAnt = -1;         // publica el estado enseguida
    } else {
      Serial.print(F("[MQTT] fallo, estado="));
      Serial.print(mqtt.state());
      Serial.println(F(" (¿IP y firewall 1883 ok?)  reintento en 20 s"));
    }
    return;
  }

  // conectado: publica al cambiar algo, y como latido cada 30 s
  unsigned long ahora = millis();
  bool cambio = (mqttLibresAnt != plazasLibres()) ||
                (mqttBarrAnt != barreraAbierta) ||
                (mqttLcdAnt  != lcdModo);
  if (mqttLibresAnt == -1 || cambio || ahora - mqttProxLatido > 30000) {
    mqttPublicarEstado();
    mqttProxLatido = ahora;
    mqttLibresAnt = plazasLibres();
    mqttBarrAnt = barreraAbierta;
    mqttLcdAnt  = lcdModo;
  }
}

#endif  // PARKNET_MQTT_H

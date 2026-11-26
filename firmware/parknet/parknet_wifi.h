// =====================================================
// parknet_wifi.h - Servidor web del Arduino (modo WiFi)
//
// Solo se incluye en placas CON WiFi (el UNO R4 WiFi). En el UNO clasico
// (AVR) este archivo no se incluye y todo sigue funcionando por USB.
//
//   Rutas:  GET  /            -> el panel web
//           GET  /api/state   -> el estado en JSON
//           POST /api/send    -> un comando JSON (los mismos que por USB)
//
// En el .ino se incluye asi:
//     #if USAR_WIFI && !defined(__AVR__)
//       #include "parknet_wifi.h"
//     #endif
// =====================================================
#pragma once

#include <WiFiS3.h>        // UNO R4 WiFi   (si usas un ESP32: #include <WiFi.h>)
#include <WiFiClient.h>
#include <WiFiServer.h>

// El UNO R4 usa beginAP(); en el ESP32 seria WiFi.softAP()
#define WIFI_AP_FUNC(s, p) WiFi.beginAP(s, p)

WiFiServer   wifiServ(WIFI_PUERTO);
WiFiClient   wifiCli;
String       wifiPeticion = "";
String       wifiCabecera = "";
String       wifiCuerpo  = "";
bool         wifiProcesado = false;

// --- el panel: es EXACTAMENTE el mismo dashboard.html del PC ---
// (se genera con gen-panel.ps1 a partir de dashboard.html)
#include "parknet_panel.h"


// =====================================================
//  COMO FUNCIONA EL WIFI DEL ARDUINO (modo automatico)
//
//  1) Intenta unirse a TU red (el hotspot "HONOR ola"). Si entra, el movil
//     (que ya esta en esa red) abre la IP del Arduino.
//  2) Si no puede (no hay hotspot, la clave no cuadra o no hay internet)
//     levanta SU PROPIA red "ParkNet": el movil se conecta ahi y abre
//     http://192.168.4.1 .  ASI SIEMPRE HAY UNA FORMA DE ENTRAR,
//     sin PC y sin internet.
//  3) Sigue buscando tu red cada 60 s sin bloquear nada (la LCD nunca
//     se congela). En cuanto "HONOR ola" aparece, se une solo.
// =====================================================

bool wifiConectado        = false;   // estamos dentro de la red del usuario
bool wifiAPActivo        = false;   // nuestra red propia esta levantada
bool wifiRedVisible       = false;   // hemos visto la red del usuario al escanear
unsigned long wifiUltimoIntento  = 0;
unsigned long wifiProximoIntento = 0;
unsigned long wifiProximoEscaneo = 0;

// ---- REDES WIFI CONOCIDAS -------------------------------------------------
// La principal es WIFI_SSID/WIFI_PASS (en parknet.ino). Aqui puedes anadir hasta 2
// mas de respaldo (casa, instituto, feria...). El Arduino se une a la primera que vea.
// Deja "" si no usas esa casilla.  (Tambien puedes definirlas en parknet.ino.)
#ifndef WIFI_SSID2
  #define WIFI_SSID2 ""
  #define WIFI_PASS2 ""
#endif
#ifndef WIFI_SSID3
  #define WIFI_SSID3 ""
  #define WIFI_PASS3 ""
#endif
struct RedWifi { const char* ssid; const char* pass; };
RedWifi wifiRedes[3] = { { WIFI_SSID, WIFI_PASS }, { WIFI_SSID2, WIFI_PASS2 }, { WIFI_SSID3, WIFI_PASS3 } };
int wifiRedActual = 0;        // la proxima red a probar
int wifiRedVisibleIdx = -1;   // la red conocida que se vio al escanear

void wifiEscanear();          // (definida mas abajo)

// Intenta unirse a la red del usuario. NO bloquea: el modulo trabaja solo
// y comprobamos el resultado en las siguientes vueltas del loop.
void wifiIntentarSTA() {
  int j = wifiRedVisibleIdx;                    // primero la que se ha visto en el aire
  if (j < 0) {                                  // si no, vamos rotando por las que tengan nombre
    for (int k = 0; k < 3; k++) {
      int c = (wifiRedActual + k) % 3;
      if (wifiRedes[c].ssid[0]) { j = c; break; }
    }
    if (j < 0) j = 0;
    wifiRedActual = (j + 1) % 3;
  }
  Serial.print(F("[WIFI] buscando '"));
  Serial.print(wifiRedes[j].ssid);
  Serial.println(F("'..."));
  WiFi.begin(wifiRedes[j].ssid, wifiRedes[j].pass);
}

// Levanta nuestra propia red "ParkNet" (el plan B: siempre se puede entrar)
void wifiLevantarAP() {
  wifiAPActivo = true;
  WIFI_AP_FUNC(WIFI_AP_SSID, WIFI_AP_PASS);
  Serial.println(F("[WIFI] ----------------------------------------------"));
  Serial.print(F("[WIFI]  RED PROPIA: "));
  Serial.print(WIFI_AP_SSID);
  Serial.print(F("   CLAVE: "));
  Serial.println(WIFI_AP_PASS);
  Serial.println(F("[WIFI]  conectate a esa red y abre http://192.168.4.1"));
  Serial.println(F("[WIFI] ----------------------------------------------"));
}

// Arranque del WiFi
void wifiConectar() {
  Serial.println(F("[WIFI] ParkNet arrancando..."));
  Serial.print(F("[WIFI] 1) me uno a tu red: "));
  Serial.println(WIFI_SSID);
  delay(1200);
  wifiEscanear();        // con el modulo en reposo: asi si lista las redes
  wifiServ.begin();
#if WIFI_MODO_AP
  wifiLevantarAP();     // modo forzado: solo red propia
#else
  wifiIntentarSTA();
  // si en 25 s no entra, levantamos nuestra propia red
  wifiProximoIntento = millis() + 25000;
  wifiProximoEscaneo = millis() + 120000;
#endif
}

// Se llama en cada vuelta del loop. No bloquea NADA.
void wifiReintentar() {
#if WIFI_MODO_AP
  return;
#else
  // 0) el modulo se conecta solo en segundo plano: si ya estamos dentro y aun no nos
  //    habiamos dado cuenta, lo damos por bueno (antes se perdia y acababa en red propia).
  //    Ojo: status() puede decir WL_CONNECTED antes de que el DHCP entregue la IP;
  //    esperamos a tener IP real para no anunciar 0.0.0.0.
  if (!wifiConectado && !wifiAPActivo && WiFi.status() == WL_CONNECTED &&
      (uint32_t)WiFi.localIP() != 0) {
    wifiConectado = true;
    Serial.print(F("[WIFI] CONECTADO a la red. IP del Arduino: "));
    Serial.println(WiFi.localIP());
    return;
  }

  // A) ya estamos dentro de la red del usuario -> solo vigilamos que no se caiga
  if (wifiConectado) {
    if (WiFi.status() == WL_CONNECTED) return;
    wifiConectado = false;
    Serial.println(F("[WIFI] nos han caido de la red, buscando otra vez..."));
  }

  unsigned long ahora = millis();

  // B) si tenemos nuestra red propia levantada, miramos de vez en cuando
  //    si aparece la red del usuario (sin tumbar la nuestra)
  if (wifiAPActivo && ahora >= wifiProximoEscaneo) {
    wifiProximoEscaneo = ahora + 120000;
    wifiEscanear();
    if (!wifiRedVisible) {
      Serial.println(F("[WIFI] tu red no esta activa de momento"));
      wifiProximoIntento = ahora + 120000;
      return;
    }
    Serial.println(F("[WIFI] tu red ha aparecido: me uno..."));
  }

  if (ahora < wifiProximoIntento) return;

  // C) intento de conexion (no bloqueante: el modulo sigue buscando solo)
  wifiIntentarSTA();
  wifiProximoIntento = ahora + 60000;

  // C2) si ha entrado y ya tiene IP, avisamos
  if (WiFi.status() == WL_CONNECTED && (uint32_t)WiFi.localIP() != 0 && !wifiConectado) {
    wifiConectado = true;
    wifiAPActivo = false;
    Serial.print(F("[WIFI] CONECTADO a la red. IP del Arduino: "));
    Serial.println(WiFi.localIP());
    Serial.println(F("[WIFI] abre esa IP en el navegador del movil"));
    return;
  }

  // D) si no entra en la red del usuario, dejamos la NUESTRA bien levantada
  //    (el intento en modo STA puede haber tumbado el AP: lo relanzamos)
  if (WiFi.status() != WL_CONNECTED) {
    if (wifiAPActivo) Serial.println(F("[WIFI] restaurando la red propia..."));
    delay(600);
    wifiLevantarAP();
  }
#endif
}

// Lista las redes que ve y con que cifrado (para diagnosticar la clave)
void wifiEscanear() {
  int n = WiFi.scanNetworks();
  wifiRedVisible = false;
  wifiRedVisibleIdx = -1;
  Serial.print(F("[WIFI] redes vistas: "));
  Serial.println(n);
  for (int i = 0; i < n && i < 12; i++) {
    Serial.print(F("[WIFI]   '"));
    Serial.print(WiFi.SSID(i));
    Serial.print(F("'  cifrado: "));
    Serial.print(WiFi.encryptionType(i));
    Serial.println();
    for (int j = 0; j < 3; j++) {
      if (wifiRedes[j].ssid[0] && strcmp(WiFi.SSID(i), wifiRedes[j].ssid) == 0) {
        wifiRedVisible = true;
        if (wifiRedVisibleIdx < 0) wifiRedVisibleIdx = j;
      }
    }
  }
  Serial.println(F("[WIFI]   (0=abierta 1=WEP 2=WPA 3=WPA2 4=WPA/WPA2 5=WPA3 6=WPA2/WPA3)"));
}

void wifiResponder(int codigo, const String& tipo, const String& cuerpo) {
  wifiCli.print(F("HTTP/1.1 "));
  wifiCli.print(codigo);
  wifiCli.print(F(" "));
  wifiCli.println(F("OK"));
  wifiCli.print(F("Content-Type: "));
  wifiCli.println(tipo);
  wifiCli.print(F("Content-Length: "));
  wifiCli.println(cuerpo.length());
  wifiCli.println(F("Connection: close"));
  wifiCli.println();
  wifiCli.print(cuerpo);
}

// Envia el panel completo (25 KB) por trozos: sin copiarlo a la RAM
void wifiResponderPanel() {
  wifiCli.println(F("HTTP/1.1 200 OK"));
  wifiCli.println(F("Content-Type: text/html; charset=utf-8"));
  wifiCli.print(F("Content-Length: "));
  wifiCli.println(PANEL_HTML_LEN);
  wifiCli.println(F("Connection: close"));
  wifiCli.println();
  unsigned int off = 0;
  while (off < PANEL_HTML_LEN) {
    unsigned int n = PANEL_HTML_LEN - off;
    if (n > 400) n = 400;
    wifiCli.write((const uint8_t *)(PANEL_HTML + off), n);
    off += n;
  }
}
void wifiServidor() {
  if (!wifiCli || !wifiCli.connected()) {
    wifiCli = wifiServ.available();
    if (!wifiCli) return;
    wifiPeticion = ""; wifiCabecera = ""; wifiCuerpo = ""; wifiProcesado = false;
    unsigned long t0 = millis();
    // No condicionar a connected(): curl/PowerShell pueden cerrar su lado de
    // escritura al terminar la peticion y el WiFiClient pasa a "desconectado"
    // aunque las cabeceras y el cuerpo sigan en el buffer. Leemos por tiempo
    // hasta la linea en blanco.
    while (millis() - t0 < 1500) {
      String ln = wifiCli.readStringUntil('\n');
      // una linea vacia (solo el \r) marca el final de las cabeceras.
      // OJO: si no se corta aqui, el bucle se come el CUERPO de la peticion.
      if (ln.length() <= 1) break;
      if (ln.startsWith("GET ") || ln.startsWith("POST ")) wifiPeticion = ln;
      wifiCabecera += ln + "\n";
      if (wifiCabecera.length() > 400) break;
    }
    if (wifiPeticion.startsWith("POST")) {
      int pos = wifiCabecera.indexOf("Content-Length:");
      if (pos < 0) pos = wifiCabecera.indexOf("content-length:");
      int len = 0;
      if (pos >= 0) len = wifiCabecera.substring(pos + 15).toInt();
      // el cuerpo puede tardar unos ms en llegar: esperamos (con margen)
      unsigned long tb = millis();
      while ((int)wifiCuerpo.length() < len && millis() - tb < 1500) {
        if (wifiCli.available()) wifiCuerpo += (char)wifiCli.read();
        else delay(5);
      }
      Serial.print(F("[WIFI] POST "));
      Serial.println(wifiPeticion);
      Serial.print(F("[WIFI]   cuerpo ("));
      Serial.print(wifiCuerpo.length());
      Serial.print(F("/"));
      Serial.print(len);
      Serial.print(F("): "));
      Serial.println(wifiCuerpo);
    }
  }
  if (wifiProcesado) return;
  wifiProcesado = true;

  if (wifiPeticion.startsWith("GET /api/state")) {
    char buf[160];
    snprintf(buf, sizeof(buf), "{\"ocupadas\":[%d,%d,%d],\"libres\":%d,\"total\":%d,\"barrera\":\"%s\",\"lcd\":\"%s\"}",
                     slotOccupied[0] ? 1 : 0, slotOccupied[1] ? 1 : 0, slotOccupied[2] ? 1 : 0,
                     plazasLibres(), NUM_SLOTS,
                     barreraAbierta ? "ABIERTA" : "CERRADA",
                     lcdModo ? "fixmol" : "datos");
    wifiResponder(200, "application/json", String(buf));
  } else if (wifiPeticion.startsWith("POST /api/send")) {
    if (wifiCuerpo.length() > 0) {
      char cmd[64];
      strncpy(cmd, wifiCuerpo.c_str(), sizeof(cmd) - 1);
      cmd[sizeof(cmd) - 1] = '\0';
      ejecutarComando(cmd);
    }
    wifiResponder(200, "application/json", F("{\"ok\":true}"));
  } else if (wifiPeticion.startsWith("GET /api/")) {
    wifiResponder(404, "application/json", F("{\"ok\":false,\"error\":\"ruta desconocida\"}"));
  } else {
    wifiResponderPanel();
  }
  // la conexion se cierra: cada peticion va en su propia conexion
  wifiCli.stop();
  wifiProcesado = false;
}

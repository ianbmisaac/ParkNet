#include <WiFiS3.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <Servo.h>

// ====== CAMBIAR ESTOS DATOS ======
const char* ssid = "HONOR ola";              // red del grupo (2.4 GHz) o hotspot del movil
const char* pass = "quesesoo";
const char* mqtt_server = "192.168.1.100";   // IP del PC con Mosquitto (ipconfig)
const int mqtt_port = 1883;
const char* CLIENT_ID = "arduino-r4-grupo1"; // debe ser unico

// Topicos: cambia "grupo1" por el nombre del proyecto (ver lista de la clase)
const char* TOPIC_TEMP   = "iot/grupo1/temperatura";
const char* TOPIC_HUM    = "iot/grupo1/humedad";
const char* TOPIC_SERVO  = "iot/grupo1/servo";
const char* TOPIC_ESTADO = "iot/grupo1/estado";
// =================================

#define DHTPIN 2
// OJO Fixmol: el servo del PARKING esta en A0 (col 48 de la protoboard),
// no en el pin 9 de la guia. Con este cambio, este sketch mueve la
// barrera de verdad. Si montas el servo en otro pin, cambialo aqui.
#define SERVOPIN A0

DHT dht(DHTPIN, DHT22);
Servo servo;
WiFiClient wifiClient;
PubSubClient client(wifiClient);

void callback(char* topic, byte* payload, unsigned int len) {
  String msg;
  for (unsigned int i = 0; i < len; i++) msg += (char)payload[i];
  msg.trim();
  Serial.println(String(topic) + " -> " + msg);

  if (String(topic) == TOPIC_SERVO) {
    if (msg == "ABRIR") servo.write(90);
    else if (msg == "CERRAR") servo.write(0);
    else servo.write(constrain(msg.toInt(), 0, 180));
    client.publish(TOPIC_ESTADO, ("servo:" + msg).c_str());
  }
}

void conectarWiFi() {
  Serial.print("Conectando a WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(ssid, pass);
    Serial.print(".");
    delay(3000);
  }
  Serial.println("\nWiFi OK, IP: " + WiFi.localIP().toString());
}

void conectarMQTT() {
  while (!client.connected()) {
    Serial.print("Conectando a MQTT...");
    if (client.connect(CLIENT_ID)) {
      Serial.println("OK");
      client.subscribe(TOPIC_SERVO);
      client.publish(TOPIC_ESTADO, "conectado");
    } else {
      Serial.println("fallo, estado=" + String(client.state()) + ", reintento en 3s");
      delay(3000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  dht.begin();
  servo.attach(SERVOPIN);
  servo.write(0);
  conectarWiFi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) conectarWiFi();
  if (!client.connected()) conectarMQTT();
  client.loop();

  static unsigned long t = 0;
  if (millis() - t > 5000) {
    t = millis();
    float temp = dht.readTemperature();
    float hum = dht.readHumidity();
    if (!isnan(temp) && !isnan(hum)) {
      client.publish(TOPIC_TEMP, String(temp).c_str());
      client.publish(TOPIC_HUM, String(hum).c_str());
      Serial.println("T=" + String(temp) + "  H=" + String(hum));
    } else {
      Serial.println("Error leyendo DHT22");
    }
  }
}

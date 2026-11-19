// ============================================================
// ParkNet - puente MQTT (broker local) -> Serie USB (COM3/Arduino)
// ------------------------------------------------------------
// Escucha comandos en iot/fixmol/comando (y iot/fixmol/servo)
// del broker local y los reenvía al Arduino por USB a través
// del servidor serie local (http://localhost:8765/api/send).
//
// Flujo:
//   admin.html -> MQTT (broker publico) -> mqtt-bridge.js ->
//   broker local 1883 -> este puente -> POST /api/send -> USB -> Arduino
//
// Ejecutar: NODE_PATH=<node_modules> node tools/mqtt-serie-bridge.js
// ============================================================
const http = require('http');
let mqtt;
try {
  mqtt = require('mqtt');
} catch (e) {
  module.paths.unshift('C:\\Users\\pexji\\AppData\\Local\\ParkNet\\node_modules');
  mqtt = require('mqtt');
}

const LOCAL_BROKER = process.env.PN_LOCAL_BROKER || 'mqtt://127.0.0.1:1883';
const SERIE_URL    = 'http://localhost:8765/api/send';
const TOPIC_COMANDO = 'iot/fixmol/comando';
const TOPIC_SERVO   = 'iot/fixmol/servo';
const TOPIC_LOG     = 'iot/fixmol/log';

let client = null;
let conectado = false;

function log(msg) {
  const now = new Date().toLocaleTimeString();
  console.log('[' + now + '] ' + msg);
}

// Envía un comando al Arduino por USB mediante el servidor serie local.
function enviarSerie(payloadStr) {
  const datos = Buffer.from(payloadStr, 'utf8');
  const req = http.request({
    hostname: 'localhost',
    port: 8765,
    path: '/api/send',
    method: 'POST',
    headers: {
      'Content-Type': 'application/json',
      'Content-Length': datos.length,
    },
  }, (resp) => {
    let body = '';
    resp.on('data', (c) => { body += c; });
    resp.on('end', () => {
      if (resp.statusCode === 200) {
        log('comando enviado por USB: ' + payloadStr.slice(0, 120));
      } else {
        log('error USB ' + resp.statusCode + ': ' + body.slice(0, 120));
      }
    });
  });
  req.on('error', (e) => {
    log('no se pudo enviar por USB (¿servidor serie arrancado?): ' + e.message);
  });
  req.write(datos);
  req.end();
}

function conectarMqtt() {
  if (client) { try { client.end(true); } catch (e) {} }
  client = mqtt.connect(LOCAL_BROKER, {
    clientId: 'pn-mqtt-serie-bridge-' + Math.random().toString(16).slice(2, 8),
    connectTimeout: 5000,
    reconnectPeriod: 4000,
    keepalive: 30,
    clean: true,
  });
  client.on('connect', () => {
    conectado = true;
    client.subscribe([TOPIC_COMANDO, TOPIC_SERVO], (err) => {
      if (err) { log('error suscribiendo: ' + err.message); return; }
      log('conectado al broker local ' + LOCAL_BROKER);
      log('suscrito a ' + TOPIC_COMANDO + ' y ' + TOPIC_SERVO);
    });
  });
  client.on('message', (topic, payload) => {
    const txt = payload.toString();
    log('comando MQTT [' + topic + '] ' + txt.slice(0, 160));
    // Reenviar al Arduino por USB tal cual (incluye el token 'k').
    enviarSerie(txt);
  });
  client.on('close', () => { conectado = false; log('desconectado del broker'); });
  client.on('error', (e) => { conectado = false; log('error broker: ' + e.message); });
  client.on('offline', () => { conectado = false; });
}

conectarMqtt();

log('iniciado: MQTT broker local -> USB serie (COM3)');

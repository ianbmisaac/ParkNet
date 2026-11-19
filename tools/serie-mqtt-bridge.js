// ============================================================
// ParkNet - puente Serie (USB/COM3) -> MQTT broker local
// ------------------------------------------------------------
// Lee el estado del Arduino desde el servidor serie local
// (http://localhost:8765/api/state) y lo publica en
// iot/fixmol/estado del broker local (mqtt://127.0.0.1:1883).
//
// El puente mqtt-bridge.js (ya en marcha) replica iot/fixmol/#
// del broker local a los brokers publicos (WSS), así la página
// de GitHub Pages recibe la ocupación real de la placa.
//
// Ejecutar: NODE_PATH=<node_modules> node tools/serie-mqtt-bridge.js
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
const TOPIC_ESTADO = 'iot/fixmol/estado';
const TOPIC_PLAZAS = 'iot/fixmol/plazas';
const TOPIC_LOG    = 'iot/fixmol/log';
const SERIE_URL    = 'http://localhost:8765/api/state';
const POLL_MS      = 2000;

let lastPayload = null;
let lastPlazas  = null;
let client = null;
let conectado = false;

function log(msg) {
  const now = new Date().toLocaleTimeString();
  console.log('[' + now + '] ' + msg);
}

function conectarMqtt() {
  if (client) { try { client.end(true); } catch (e) {} }
  client = mqtt.connect(LOCAL_BROKER, {
    clientId: 'pn-serie-bridge-' + Math.random().toString(16).slice(2, 8),
    connectTimeout: 5000,
    reconnectPeriod: 4000,
    keepalive: 30,
    clean: true,
  });
  client.on('connect', () => {
    conectado = true;
    log('conectado al broker local ' + LOCAL_BROKER);
  });
  client.on('connect', () => { conectado = true; });
  client.on('close', () => { conectado = false; log('desconectado del broker'); });
  client.on('error', (e) => { conectado = false; log('error broker: ' + e.message); });
  client.on('offline', () => { conectado = false; });
}

function publicarEstado(obj) {
  if (!conectado || !client) return;
  // Añadimos un timestamp para que cada mensaje sea distinto y el puente
  // mqtt-bridge (anti-bucle por topic+payload) lo replique a los brokers
  // públicos aunque el estado de la placa no cambie.
  obj.ts = Date.now();
  const payload = JSON.stringify(obj);
  // retain: true => el broker conserva el último estado; cualquier suscriptor
  // nuevo (página web) lo recibe nada más conectarse.
  client.publish(TOPIC_ESTADO, payload, { qos: 0, retain: true });
  lastPayload = payload;
  log('publicado ' + TOPIC_ESTADO + ' (retain) ocupadas=' + JSON.stringify(obj.ocupadas) +
    ' libres=' + obj.libres + ' barrera=' + obj.barrera + ' ts=' + obj.ts);
  if (typeof obj.libres === 'number') {
    const txt = String(obj.libres);
    client.publish(TOPIC_PLAZAS, txt, { qos: 0, retain: true });
    lastPlazas = txt;
  }
}

function logMqtt(linea) {
  if (conectado && client) {
    client.publish(TOPIC_LOG, '[serie] ' + linea, { qos: 0 });
  }
}

function leerEstado() {
  const req = http.get(SERIE_URL, (resp) => {
    let data = '';
    resp.on('data', (chunk) => { data += chunk; });
    resp.on('end', () => {
      try {
        const obj = JSON.parse(data);
        if (obj && Array.isArray(obj.ocupadas)) {
          publicarEstado(obj);
          logMqtt('estado placa: ocupadas=' + JSON.stringify(obj.ocupadas) +
            ' libres=' + obj.libres + ' barrera=' + obj.barrera);
        }
      } catch (e) {
        log('error parseando estado del serie: ' + e.message);
      }
    });
  });
  req.on('error', (e) => {
    log('error consultando /api/state: ' + e.message);
  });
  req.setTimeout(3000, () => { req.destroy(); log('timeout /api/state'); });
}

conectarMqtt();
leerEstado();
setInterval(leerEstado, POLL_MS);

log('iniciado (polling ' + POLL_MS + ' ms)');

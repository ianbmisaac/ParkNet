// ============================================================
// ParkNet - puente MQTT: broker LOCAL <-> brokers PUBLICOS
// ------------------------------------------------------------
// La placa UNO R4 WiFi esta detras del hotspot de Windows sin
// NAT, asi que publica en el broker LOCAL del PC (192.168.137.1:1883).
// La pagina admin (GitHub Pages, HTTPS) entra por WebSocket a
// uno de tres brokers publicos (el primero que responda).
//
// Este proceso replica iot/fixmol/# entre TODOS los brokers,
// con anti-bucle por topic+payload. Asi da igual cual de los
// tres escoja el admin: siempre ve el estado y sus comandos
// llegan a la placa.
//
// Ejecutar: NODE_PATH=<carpeta con node_modules/mqtt> node mqtt-bridge.js
// ============================================================
// El modulo mqtt se guarda fuera de %TEMP% para que sobreviva a limpiezas.
let mqtt;
try {
  mqtt = require('mqtt');
} catch (e) {
  module.paths.unshift('C:\\Users\\pexji\\AppData\\Local\\ParkNet\\node_modules');
  mqtt = require('mqtt');
}

const URLS = [
  process.env.PN_LOCAL_BROKER || 'mqtt://127.0.0.1:1883',
  'wss://test.mosquitto.org:8081',
  'wss://broker.hivemq.com:8884/mqtt',
  'wss://broker.emqx.io:8084/mqtt',
];
const NOMBRES = ['local', 'mosquitto', 'hivemq', 'emqx'];
const TOPIC = 'iot/fixmol/#';
const VENTANA_MS = 2000;   // anti-bucle (un round-trip tarda <1 s)

const vistos = new Map();
const clients = [];

function reenviar(desde, topic, payload) {
  const clave = topic + '|' + payload.toString();
  const ahora = Date.now();
  if (ahora - (vistos.get(clave) || 0) < VENTANA_MS) return;   // eco
  vistos.set(clave, ahora);
  if (vistos.size > 800) vistos.clear();

  let n = 0;
  clients.forEach((c, i) => {
    if (i === desde || !c.connected) return;
    c.publish(topic, payload, { qos: 0 });
    n++;
  });
  if (n) {
    console.log('  [' + NOMBRES[desde] + ' -> ' + n + ' brokers] ' +
                topic + '  ' + payload.toString().slice(0, 80));
  }
}

URLS.forEach((url, i) => {
  const c = mqtt.connect(url, {
    connectTimeout: 9000,
    reconnectPeriod: 4000,
    clientId: 'pn-bridge-' + NOMBRES[i] + '-' + Math.random().toString(16).slice(2, 8),
    keepalive: 30,
    clean: true,
  });
  clients.push(c);

  c.on('connect', () => {
    console.log('[' + new Date().toLocaleTimeString() + '] ' + NOMBRES[i] + ' conectado');
    c.subscribe(TOPIC, (err) => {
      if (err) console.log('  error suscribiendo ' + NOMBRES[i] + ': ' + err.message);
    });
  });
  c.on('message', (topic, payload) => reenviar(i, topic, payload));
  c.on('error', (e) => console.log('[' + new Date().toLocaleTimeString() + '] ' +
                                   NOMBRES[i] + ' error: ' + e.message));
});

setInterval(() => {
  const corte = Date.now() - VENTANA_MS;
  for (const [k, t] of vistos) if (t < corte) vistos.delete(k);
}, 10000).unref();

console.log('ParkNet bridge listo. Replicando ' + TOPIC + ' en ' + URLS.length + ' brokers.');

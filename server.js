// server.js
const express = require('express');
const http = require('http');
const mqtt = require('mqtt');
const { WebSocketServer } = require('ws');
const path = require('path');

// --- KONFIGURASI ---
const MQTT_BROKER_URL = 'mqtt://broker.hivemq.com'; // Broker MQTT publik, bisa diganti
const MQTT_TOPIC = 'dalwa/tandon/status';
const SERVER_PORT = process.env.PORT || 3000;

// Inisialisasi Express App
const app = express();
app.use(express.static(path.join(__dirname, 'public'))); // Menyajikan file dari folder 'public'

const server = http.createServer(app);

// Inisialisasi WebSocket Server
const wss = new WebSocketServer({ server });
let lastKnownData = {}; // Menyimpan data terakhir untuk klien baru

wss.on('connection', (ws) => {
    console.log('Frontend terhubung via WebSocket');
    // Kirim data terakhir ke klien yang baru terhubung
    if (Object.keys(lastKnownData).length > 0) {
        ws.send(JSON.stringify(lastKnownData));
    }
});

function broadcast(data) {
    wss.clients.forEach((client) => {
        if (client.readyState === 1) { // 1 = OPEN
            client.send(data);
        }
    });
}

console.log('Menghubungkan ke MQTT Broker...');
const mqttClient = mqtt.connect(MQTT_BROKER_URL);

mqttClient.on('connect', () => {
    console.log(`Terhubung ke MQTT Broker di ${MQTT_BROKER_URL}`);
    mqttClient.subscribe(MQTT_TOPIC, (err) => {
        if (!err) {
            console.log(`Berhasil subscribe ke topik: ${MQTT_TOPIC}`);
        } else {
            console.error('Gagal subscribe:', err);
        }
    });
});

mqttClient.on('message', (topic, message) => {
    if (topic === MQTT_TOPIC) {
        console.log(`Pesan diterima dari ESP32: ${message.toString()}`);
        const dataString = message.toString();
        try {
            lastKnownData = JSON.parse(dataString);
            broadcast(dataString); // Teruskan data ke semua klien WebSocket
        } catch(e) {
            console.error("Gagal parsing JSON dari ESP32:", e);
        }
    }
});

mqttClient.on('error', (error) => {
    console.error('Koneksi MQTT Error:', error);
});

server.listen(SERVER_PORT, () => {
    console.log(`Server berjalan di http://localhost:${SERVER_PORT}`);
});

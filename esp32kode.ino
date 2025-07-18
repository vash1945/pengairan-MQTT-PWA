#include <WiFi.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// --- KONFIGURASI JARINGAN & MQTT ---
const char* mqtt_server = "broker.hivemq.com"; // Gunakan broker yang sama dengan backend
const int mqtt_port = 1883;
const char* mqtt_topic = "dalwa/tandon/status"; // Gunakan topik yang sama dengan backend
const char* client_id_prefix = "dalwa-tandon-";

// --- KONFIGURASI PIN ---
const int PIN_FLOAT_ATAS = 12;
const int PIN_FLOAT_TENGAH = 13;
const int PIN_FLOAT_BAWAH = 14;
const int PIN_LED = 2;

// --- KONFIGURASI INTERVAL ---
const unsigned long INTERVAL_CEK_AIR = 5000; // Cek setiap 5 detik
unsigned long lastCekAir = 0;

// --- GLOBAL OBJECTS ---
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// --- STATUS TRACKING ---
String lastStatusJson = "";

// Fungsi untuk membaca sensor (Active LOW karena INPUT_PULLUP)
// Mengembalikan `true` jika terendam air (terhubung ke GND)
bool bacaSensor(int pin) {
    return !digitalRead(pin);
}

void setupMqtt() {
    String clientId = String(client_id_prefix) + WiFi.macAddress();
    mqttClient.setServer(mqtt_server, mqtt_port);
    Serial.printf("Mencoba terhubung ke MQTT Broker dengan Client ID: %s\n", clientId.c_str());

    if (mqttClient.connect(clientId.c_str())) {
        Serial.println("Berhasil terhubung ke MQTT Broker!");
    } else {
        Serial.print("Gagal terhubung ke MQTT, status rc=");
        Serial.print(mqttClient.state());
        Serial.println(" Mencoba lagi dalam 5 detik...");
        // Tidak menggunakan delay() agar tidak memblokir
    }
}

void setupWifi() {
    WiFi.mode(WIFI_STA);
    WiFiManager wm;
    wm.setConnectTimeout(30);
    wm.setConfigPortalTimeout(180);

    bool isConnected = wm.autoConnect("TandonAir_Setup", "12345678");
    if (!isConnected) {
        Serial.println("Gagal terhubung WiFi, restart dalam 10 detik...");
        delay(10000);
        ESP.restart();
    }
    Serial.println("Berhasil terhubung ke WiFi!");
    Serial.print("Alamat IP: ");
    Serial.println(WiFi.localIP());
}

void publishStatus() {
    bool atas = bacaSensor(PIN_FLOAT_ATAS);
    bool tengah = bacaSensor(PIN_FLOAT_TENGAH);
    bool bawah = bacaSensor(PIN_FLOAT_BAWAH);

    int levelPersen = 0;
    String statusTeks = "SEDANG";

    // Konversi status sensor ke persentase (estimasi)
    if (!atas && !tengah && !bawah) {
        levelPersen = 100;
        statusTeks = "PENUH";
    }   else if (atas && tengah && bawah) {
        levelPersen = 5;
        statusTeks = "HABIS";
    }   else {
        levelPersen = 50;
        statusTeks = "SEDANG";
    }

    // Buat JSON payload
    JsonDocument doc;
    doc["level"] = levelPersen;
    doc["status"] = statusTeks;
    doc["sensorAtas"] = atas;
    doc["sensorTengah"] = tengah;
    doc["sensorBawah"] = bawah;

    String outputJson;
    serializeJson(doc, outputJson);

    // Hanya publish jika ada perubahan data untuk efisiensi
    if (outputJson != lastStatusJson) {
        Serial.printf("Data berubah, mempublikasikan: %s\n", outputJson.c_str());

        bool published = mqttClient.publish(mqtt_topic, outputJson.c_str(), true); // `true` untuk retained message
        if (published) {
            Serial.println("Pesan berhasil dipublikasikan!");
            lastStatusJson = outputJson;
            digitalWrite(PIN_LED, !digitalRead(PIN_LED)); // Blink LED sebagai indikator publish
            delay(50);
            digitalWrite(PIN_LED, !digitalRead(PIN_LED));
        } else {
            Serial.println("Gagal mempublikasikan pesan.");
        }
    }
}

void setup() {
    Serial.begin(115200);

    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, LOW); // LED nyala saat setup

    pinMode(PIN_FLOAT_ATAS, INPUT_PULLUP);
    pinMode(PIN_FLOAT_TENGAH, INPUT_PULLUP);
    pinMode(PIN_FLOAT_BAWAH, INPUT_PULLUP);

    setupWifi();
    setupMqtt();

    digitalWrite(PIN_LED, HIGH); // LED mati setelah setup selesai
}

void loop() {
    // Jika koneksi WiFi atau MQTT terputus, coba sambungkan kembali
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Koneksi WiFi terputus. Mencoba menyambungkan kembali...");
        setupWifi();
    }

    if (!mqttClient.connected()) {
        static unsigned long lastMqttRetry = 0;
        if (millis() - lastMqttRetry > 5000) {
            lastMqttRetry = millis();
            Serial.println("Koneksi MQTT terputus. Mencoba menyambungkan kembali...");
            setupMqtt();
        }
    }

    // Jaga koneksi MQTT tetap hidup
    mqttClient.loop();

    // Cek dan kirim status air sesuai interval
    if (millis() - lastCekAir > INTERVAL_CEK_AIR) {
        lastCekAir = millis();
        if (mqttClient.connected()) {
            publishStatus();
        }
    }
}

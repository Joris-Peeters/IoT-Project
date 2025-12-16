#include <Adafruit_BME280.h>
#include <Adafruit_Sensor.h>
#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <Wire.h>
#include "secrets.h"

// ---- CONFIG ----
#define INTERVAL 10000

const char* mqttHost = "vpn.jpsystems.xyz";
const int mqttPort = 1883;

// Topics
const char* TELEMETRY_TOPIC = "v1/devices/me/telemetry";
const char* RPC_REQ_TOPIC = "v1/devices/me/rpc/request/+";
const char* RPC_RESP_TOPIC = "v1/devices/me/rpc/response/";

// ---- HW ----
#define LED_PIN 32
#define LDR_PIN 36

// ---- GLOBALS ----
WiFiClient espClient;
PubSubClient mqtt(espClient);
unsigned long lastSend = 0;

Adafruit_BME280 bme;

float tVal, hVal, pVal;
int analogVal;
bool ledState = false;

// ---- RPC HANDLER ----
void handleRpc(char* topic, byte* payload, unsigned int len) {
    String data;
    for (int i = 0; i < len; i++) data += (char)payload[i];

    // Find request ID from topic
    String t = String(topic);
    String reqId = t.substring(t.lastIndexOf('/') + 1);

    // Parse LED command
    data.toLowerCase();
    if (data.indexOf("toggle") != -1) {
        digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    } else if (data.indexOf("on") != -1) {
        digitalWrite(LED_PIN, HIGH);
    } else if (data.indexOf("off") != -1) {
        digitalWrite(LED_PIN, LOW);
    }

    // Send response back (optional but nice)
    String respTopic = String(RPC_RESP_TOPIC) + reqId;
    String resp = String(digitalRead(LED_PIN) ? "true" : "false");
    mqtt.publish(respTopic.c_str(), resp.c_str());
}

void update_sensors() {
    tVal = bme.readTemperature();
    hVal = bme.readHumidity();
    pVal = bme.readPressure();
    analogVal = analogRead(LDR_PIN);
}

void connectWiFi() {
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) delay(200);
}

void connectMQTT() {
    while (!mqtt.connected()) {
        if (mqtt.connect("ESP32EnvSens", token, NULL)) {
            mqtt.subscribe(RPC_REQ_TOPIC);
        } else {
            delay(1000);
        }
    }
}

void setup() {
    Serial.begin(9600);
    while (!Serial);  // time to get serial running

    if (!bme.begin(0x76)) {
        while (1) delay(10);
    }

    pinMode(LED_PIN, OUTPUT);
    pinMode(LDR_PIN, INPUT);

    connectWiFi();

    mqtt.setServer(mqttHost, mqttPort);
    mqtt.setCallback(handleRpc);
    connectMQTT();
}

void loop() {
    if (!mqtt.connected()) connectMQTT();
    mqtt.loop();

    unsigned long now = millis();
    if (now - lastSend > INTERVAL) {
        lastSend = now;

        update_sensors();

        String payload =
            "{\"temperature\":" + String(tVal) +
            ",\"humidity\":" + String(hVal) +
            ",\"pressure\":" + String(pVal) +
            ",\"analog\":" + String(analogVal) +
            "}";

        mqtt.publish(TELEMETRY_TOPIC, payload.c_str());
    }
}
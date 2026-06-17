#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "MAX30105.h"
#include <PubSubClient.h>

const char* ssid = "Fibre_inwi_6CB5";
const char* pass = "48DC2D8D0E70";
const char* mqtt_server = "broker.emqx.io";
const int   mqtt_port   = 1883;
const char* mqtt_topic_bpm  = "pfe/medical/bpm";
const char* mqtt_topic_spo2 = "pfe/medical/spo2";
const char* mqtt_client_id  = "ESP32_MAX30102_001"; 

Adafruit_SSD1306 display(128, 64, &Wire, -1);
MAX30105 particleSensor;
WiFiClient espClient;
PubSubClient mqtt(espClient);

float smoothedBPM = 0; long lastBeatTime = 0; int spo2 = 0;
static float movingAvg = 0; unsigned long lastPublishTime = 0;

void reconnectMQTT() {
  while (!mqtt.connected()) {
    Serial.print("Connexion au serveur MQTT...");
    if (mqtt.connect(mqtt_client_id)) { Serial.println(" Connecté !"); } 
    else { delay(3000); }
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  particleSensor.setup(0x1F, 4, 2, 100, 411, 4096);
  WiFi.begin(ssid, pass);
  mqtt.setServer(mqtt_server, mqtt_port);
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!mqtt.connected()) { reconnectMQTT(); }
    mqtt.loop();
  }
  long irValue  = particleSensor.getIR();
  long redValue = particleSensor.getRed();

  if (irValue > 50000) { // Détection de présence du doigt
    movingAvg = (movingAvg * 0.96) + (irValue * 0.04);
    if (irValue > (movingAvg + 150) && (millis() - lastBeatTime > 750)) {
      long delta = millis() - lastBeatTime;
      lastBeatTime = millis();
      float instantBPM = 60000.0 / delta;
      if (instantBPM > 45 && instantBPM < 180) {
        smoothedBPM = (smoothedBPM == 0) ? instantBPM : (smoothedBPM * 0.9) + (instantBPM * 0.1);
      }
  }
    double ratio = (double)redValue / (double)irValue;
    spo2 = 110 - (17 * ratio);
    if (spo2 > 100) spo2 = 100; if (spo2 < 85) spo2 = 95;

    // Affichage local et publication MQTT toutes les 2 secondes
    if (millis() - lastPublishTime > 2000 && smoothedBPM > 45) {
      if (WiFi.status() == WL_CONNECTED && mqtt.connected()) {
        char b[8], s[8]; dtostrf(smoothedBPM, 4, 1, b); dtostrf(spo2, 4, 1, s);
        mqtt.publish(mqtt_topic_bpm, b); mqtt.publish(mqtt_topic_spo2, s);
      }
      lastPublishTime = millis();
    }
  } else { // Retrait du doigt
    if (smoothedBPM != 0 || spo2 != 0) {
      smoothedBPM = 0; spo2 = 0;
      if (WiFi.status() == WL_CONNECTED && mqtt.connected()) {
        mqtt.publish(mqtt_topic_bpm, "0.0"); mqtt.publish(mqtt_topic_spo2, "0.0");
      }
    }
  }
  delay(100);
}
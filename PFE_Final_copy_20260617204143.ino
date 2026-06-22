#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "MAX30105.h"
#include <PubSubClient.h>

// --- VOS IDENTIFIANTS WI-FI 2.4GHz ---
const char* ssid = "WIFI NAME";
const char* pass = "PASS";

// --- CONFIGURATION MQTT ---
const char* mqtt_server = "broker.emqx.io";
const int mqtt_port = 1883;
const char* mqtt_topic_bpm = "pfe/medical/bpm";
const char* mqtt_topic_spo2 = "pfe/medical/spo2";
const char* mqtt_client_id = "ESP32_MAX30102_001";

// --- INSTANCIATION DES PERIPHERIQUES ---

Adafruit_SSD1306 display(128, 64, &Wire, -1);
MAX30105 particleSensor;
WiFiClient espClient;
PubSubClient mqtt(espClient);



// --- VARIABLES GLOBALES DE CALCUL ---

float smoothedBPM = 0;
long lastBeatTime = 0;
int spo2 = 0;
static float movingAvg = 0;
unsigned long lastPublishTime = 0;

void reconnectMQTT() {

  if (!mqtt.connected()) {
    Serial.print("Tentative de connexion MQTT...");
    if (mqtt.connect(mqtt_client_id)) {
      Serial.println(" Connecté au Broker !");
    } else {
      Serial.print(" Échec, code d'état: ");
      Serial.println(mqtt.state());
    }
  }
}



void setup() {
  Serial.begin(115200);
  delay(1000);
  // Initialisation du bus I2C (SDA = GPIO 21, SCL = GPIO 22)
  Wire.begin(21, 22);

  // 1. Initialisation de l'écran OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("Écran OLED non trouvé !");
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 10);
  display.println("MEDISENSE");
  display.println("Initialisation Wi-Fi...");
  display.display();
  // 2. Connexion au réseau Wi-Fi

  WiFi.mode(WIFI_STA);

  WiFi.disconnect();  

  delay(100);



  Serial.print("Connexion au Wi-Fi ");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);

 

  int tentative = 0;

  while (WiFi.status() != WL_CONNECTED && tentative < 40) {
    delay(500);
    Serial.print(".");
    tentative++;

  }

  if(WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWi-Fi Connecté !");
    Serial.print("Adresse IP : ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nÉchec Wi-Fi (Le système fonctionnera en mode local).");
  }

  // 3. Initialisation du capteur MAX30102

  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {

    Serial.println("MAX30102 introuvable ! Système bloqué.");
    display.clearDisplay();
    display.setCursor(0,10);
    display.println("ERREUR MATERIEL:");
    display.println("MAX30102 absent");
    display.display();

    while (1); // Bloque ici si le capteur est mal branché ou mal alimenté
  }

  // Configuration optimale du capteur pour la détection du pouls

  particleSensor.setup(0x1F, 4, 2, 100, 411, 4096);

  particleSensor.setPulseAmplitudeRed(0x0A); // Allume la LED rouge

  particleSensor.setPulseAmplitudeIR(0x0A);  // Allume la LED infrarouge

  // Configuration du serveur MQTT
  mqtt.setServer(mqtt_server, mqtt_port);

  display.clearDisplay();
  display.setCursor(0, 20);
  display.println("Système Prêt !");
  display.display();
  delay(1000);

}

void loop() {

  // Gestion asynchrone non-bloquante des connexions réseau

  if (WiFi.status() == WL_CONNECTED) {
    if (!mqtt.connected()) {
      reconnectMQTT();
    }
    mqtt.loop();
  }
 
  // Lecture des valeurs brutes infrarouge (IR) et Rouge (Red)

  long irValue = particleSensor.getIR();

  long redValue = particleSensor.getRed();


  // --- SEUIL DE DETECTION ABAISSÉ À 20000 POUR PLUS DE SENSIVITÉ ---

  if (irValue > 20000) {
    // Algorithme de calcul du rythme cardiaque (BPM)

    movingAvg = (movingAvg * 0.96) + (irValue * 0.04);

    if (irValue > (movingAvg + 150) && (millis() - lastBeatTime > 750)) {

      long delta = millis() - lastBeatTime;

      lastBeatTime = millis();

      float instantBPM = 60000.0 / delta;

      if (instantBPM > 45 && instantBPM < 180) {
        smoothedBPM = (smoothedBPM == 0) ? instantBPM : (smoothedBPM * 0.9) + (instantBPM * 0.1);
      }
    }

    // Algorithme de calcul de l'oxygène (SpO2)
    double ratio = (double)redValue / (double)irValue;
    spo2 = 110 - (17 * ratio);
    if (spo2 > 100) spo2 = 100;
    if (spo2 < 85) spo2 = 95;


    // --- MISE À JOUR DE L'AFFICHAGE OLED LOCAL ---
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    if (WiFi.status() == WL_CONNECTED && mqtt.connected()) {
      display.print("WiFi: OK  | MQTT: OK");
    } else {
      display.print("Mode: Local (Hors ligne)");
    }
    display.setTextSize(2);
    display.setCursor(0, 20);
    display.print("BPM : ");
    display.println((int)smoothedBPM);
    display.print("SpO2: ");
    display.print(spo2);
    display.println("%");
    display.display();


    // --- TRANSMISSION VERS NODE-RED (MQTT) TOUTES LES 2 SECONDES ---
    if (millis() - lastPublishTime > 2000 && smoothedBPM > 45) {
      if (WiFi.status() == WL_CONNECTED && mqtt.connected()) {
        char b[8], s[8];
        dtostrf(smoothedBPM, 4, 1, b);
        dtostrf(spo2, 4, 1, s);
        mqtt.publish(mqtt_topic_bpm, b);
        mqtt.publish(mqtt_topic_spo2, s);
        Serial.println("Données vitales envoyées à Node-RED via EMQX !");
      }
      lastPublishTime = millis();
    }
  }
  else {
    // Si aucun doigt n'est détecté sur le capteur
    if (smoothedBPM != 0 || spo2 != 0) {
      smoothedBPM = 0;
      spo2 = 0;
      if (WiFi.status() == WL_CONNECTED && mqtt.connected()) {
        mqtt.publish(mqtt_topic_bpm, "0.0");
        mqtt.publish(mqtt_topic_spo2, "0.0");
      }
    }
    // Message d'attente dynamique sur l'écran OLED
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 25);
    display.println("Placer votre doigt...");
    display.display();
  }
  delay(40); // Fréquence d'échantillonnage stable
} 


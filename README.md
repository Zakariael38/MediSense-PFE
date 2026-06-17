# MediSense 🏥 ── Système IoT Connecté de Télésurveillance Médicale

MediSense est une solution IoT de e-santé complète permettant la collecte, la transmission asynchrone et l'analyse intelligente des constantes physiologiques d'un patient en temps réel. Ce projet a été développé dans le cadre d'un Projet de Fin d'Études (PFE).

---

## 🚀 Architecture Globale du Système

Le système est structuré autour d'une chaîne verticale à trois niveaux :
1. **Edge (Capteur & Embarqué) :** Un microcontrôleur ESP32 acquiert les données brutes d'un capteur photopléthysmographique (PPG) MAX30102 via le bus I2C, filtre les bruits de mesure et gère un affichage local sur écran OLED SSD1306.
2. **Fog / Backend Layer :** Les données sont publiées sans fil via le protocole MQTT (Broker EMQX). Une passerelle Node-RED intercepte les flux, maintient l'état logique global et expose une API REST, rendue publique grâce à un tunnel inverse ngrok.
3. **Cloud / Frontend Layer :** Une application web asynchrone (HTML5/JavaScript) propose deux espaces distincts (Patient et Médecin) mettant à jour des graphiques dynamiques sans rechargement de page.

---

## 🧠 Fonctionnalités Avancées & Algorithmes Cliniques

*   **Calcul du SpO2 & BPM :** Traitement optique direct basé sur la loi de Beer-Lambert (LED Rouge 660 nm et Infrarouge 880 nm).
*   **Détection d'Arythmie (AFib) :** Algorithme JavaScript calculant la variabilité de la fréquence cardiaque par la méthode mathématique du **RMSSD** (seuil d'instabilité réglé à 55 ms).
*   **Évaluation du Risque NEWS2 :** Intégration de la grille clinique standardisée *National Early Warning Score* pour classifier le niveau d'urgence.
*   **Notifications d'Urgence SMTP :** Routage d'arrière-plan asynchrone et sécurisé via **Nodemailer** pour alerter instantanément le médecin par e-mail en cas de constantes critiques.

---

## 🛠️ Technologies Utilisées

| Niveau | Technologies & Matériels |
| :--- | :--- |
| **Matériel (Hardware)** | ESP32 NodeMCU, Capteur MAX30102, Écran OLED SSD1306 (Bus I2C : GPIO 21 & 22) |
| **Firmware** | C++ (IDE Arduino), Bibliothèques Adafruit GFX, PubSubClient |
| **Routage & Backend** | Node-RED, Broker MQTT (EMQX), Tunnel ngrok |
| **Application Web** | HTML5, CSS3 (Responsive Dark Mode), JavaScript Asynchrone (Fetch API / Polling) |
| **Notifications** | Node.js / Module Nodemailer (Transport SMTP TLS/SSL) |

---

## 📁 Structure du Dépôt

```text
├── firmware_esp32/
│   └── firmware_esp32.ino      # Code source C++ pour l'ESP32
├── backend_nodered/
│   └── flows.json              # Exportation des flux logiques de Node-RED
├── frontend_web/
│   ├── patient_dashboard.html  # Interface de supervision du patient
│   ├── doctor_dashboard.html   # Tableau de bord de centralisation médicale
│   └── app.js                  # Logique algorithmique (RMSSD, NEWS2, Ajax)
└── README.md                   # Documentation du projet

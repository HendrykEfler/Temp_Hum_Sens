#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <SPI.h>
#include <SD.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <TimeLib.h>    

#include "webpage.h"   // HTML auslagern

// WLAN
const char* ssid = "Vodafone-25B4";
const char* password = "qaNPdMPHxC7xmg7x";

// NodeMCU Webserver
ESP8266WebServer server(80);

// BME280 Sensor
Adafruit_BME280 bme;
float temperature, humidity, pressure;

// Messintervall
unsigned long lastMeasurement = 0;
const unsigned long MEASURE_INTERVAL = 2000; // 2 Sekunden

// LED Status
#define LED_ON  LOW
#define LED_OFF HIGH

// SD-Karte
#define SD_CS D8
unsigned long lastPrint = 0;
#define BUTTON D3
unsigned long pressStart = 0;
bool pressed = false;

// ---------- NTP ----------
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000); // UTC, 60s Update
unsigned long lastNTPUpdate = 0;

// ---------- interne Zeit-Referenz ----------
unsigned long lastMillisUpdate = 0;

// ------------------- Hilfsfunktionen -------------------

// Aktualisiert TimeLib intern basierend auf millis()
void updateInternalTime() {
  unsigned long elapsed = millis() - lastMillisUpdate;
  if (elapsed > 0) {
    adjustTime(elapsed / 1000); // Sekunden zu TimeLib addieren
    lastMillisUpdate = millis();
  }
}

void initLogFile() {
  if (!SD.exists("/data.csv")) {
    File f = SD.open("/data.csv", "w");
    f.println("Zeit,Temp,Hum,Pressure");
    f.close();
  }
}

void logMeasurement() {
  updateInternalTime();

  time_t t = now();
  char buf[20];
  sprintf(buf, "%04d-%02d-%02d %02d:%02d:%02d",
          year(t), month(t), day(t),
          hour(t), minute(t), second(t));

  File dataFile = SD.open("/data.csv", "a");
  if (dataFile) {
    dataFile.print(buf);
    dataFile.print(",");
    dataFile.print(temperature);
    dataFile.print(",");
    dataFile.print(humidity);
    dataFile.print(",");
    dataFile.println(pressure);

    dataFile.flush(); 
    dataFile.close();
  } else {
    Serial.println("Fehler beim Öffnen von data.csv");
  }
}


void measureSensor() {
  temperature = bme.readTemperature();
  humidity = bme.readHumidity();
  pressure = bme.readPressure() / 100.0F;

  digitalWrite(LED_BUILTIN, LED_ON);
  delay(50);
  digitalWrite(LED_BUILTIN, LED_OFF);

  logMeasurement();
}

void handleRoot() {
  // HTML direkt aus PROGMEM senden, ohne RAM-Overflow
  server.send_P(200, "text/html", MAIN_page);
}

void handleData() {
  updateInternalTime();
  String json = String("{\"temperature\":") + temperature +
                ",\"humidity\":" + humidity +
                ",\"pressure\":" + pressure +
                ",\"time\":\"" + now() + "\"}";
  server.send(200, "application/json", json);
}

void handleSDData() {
  File dataFile = SD.open("data.csv");
  String json = "[";
  if (dataFile) {
    bool first = true;
    while (dataFile.available()) {
      String line = dataFile.readStringUntil('\n');
      line.trim();
      if (line.length() == 0) continue;

      int firstComma = line.indexOf(',');
      int secondComma = line.indexOf(',', firstComma + 1);
      int thirdComma = line.indexOf(',', secondComma + 1);

      String timestamp = line.substring(0, firstComma);
      String temp = line.substring(firstComma + 1, secondComma);
      String hum = line.substring(secondComma + 1, thirdComma);
      String pres = line.substring(thirdComma + 1);

      if (!first) json += ",";
      first = false;

      json += "{";
      json += "\"time\":\"" + timestamp + "\",";
      json += "\"temperature\":" + temp + ",";
      json += "\"humidity\":" + hum + ",";
      json += "\"pressure\":" + pres;
      json += "}";
    }
    dataFile.close();
  }
  json += "]";
  server.send(200, "application/json", json);
}

void printCSVPreview() {
  File file = SD.open("/data.csv", "r");
  if (!file) {
    Serial.println("Datei konnte nicht geöffnet werden!");
    return;
  }

  Serial.println("\n--- CSV Vorschau ---");

  // 🔹 Erste 3 Zeilen
  Serial.println("Erste 3 Zeilen:");
  file.seek(0);

  int lineCount = 0;
  while (file.available() && lineCount < 3) {
    String line = file.readStringUntil('\n');
    Serial.println(line);
    lineCount++;
  }

  // 🔹 Letzte 3 Zeilen (Ringpuffer)
  Serial.println("\nLetzte 3 Zeilen:");
  file.seek(0);

  String lastLines[3];
  int idx = 0;
  int totalLines = 0;

  while (file.available()) {
    String line = file.readStringUntil('\n');
    lastLines[idx] = line;
    idx = (idx + 1) % 3;  // Ringpuffer
    totalLines++;
  }

  int start = (totalLines < 3) ? 0 : idx;

  for (int i = 0; i < min(3, totalLines); i++) {
    Serial.println(lastLines[(start + i) % 3]);
  }

  file.close();
  Serial.println("--- Ende Vorschau ---\n");
}

void checkDeleteButton() {
  if (digitalRead(BUTTON) == LOW) { // Taster gedrückt
    if (!pressed) {
      pressStart = millis();  // Startzeit merken
      pressed = true;
    }

    // Prüfen, ob 5 Sekunden vergangen
    if (millis() - pressStart >= 5000) {
      Serial.println("Lösche data.csv ...");

      if (SD.exists("/data.csv")) {
        SD.remove("/data.csv");
        Serial.println("data.csv gelöscht!");
      } else {
        Serial.println("Datei existiert nicht.");
      }

      // Damit nicht ständig gelöscht wird, wenn Taster weiter gedrückt
      pressed = false;
      delay(1000); // kurze Pause
    }
  } else {
    // Taster losgelassen → Reset
    pressed = false;
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin(D2, D1);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LED_OFF);

  pinMode(BUTTON,INPUT_PULLUP);

  // BME280 initialisieren
  if (!bme.begin(0x76)) {
    Serial.println("BME280 nicht gefunden!");
    while (1);
  }

  // SD-Karte initialisieren
  if (!SD.begin(SD_CS)) {
    Serial.println("SD-Karte konnte nicht initialisiert werden!");
  } else {
    Serial.println("SD-Karte bereit.");
    initLogFile();
  }
  
  // WLAN verbinden
  WiFi.begin(ssid, password);
  Serial.print("Verbinde mit WLAN");
  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
  }
  Serial.println("\nWLAN verbunden. IP: " + WiFi.localIP().toString());

  // TimeLib initialisieren (Fallback auf 0)
  setTime(0);
  lastMillisUpdate = millis();

  // NTP initialisieren
  timeClient.begin();
  if (WiFi.status() == WL_CONNECTED && timeClient.update()) {
    setTime(timeClient.getEpochTime()); // UTC-Zeit von NTP
    lastMillisUpdate = millis();
    lastNTPUpdate = millis();
    Serial.println("NTP-Zeit empfangen: " +
                   String(year()) + "-" + String(month()) + "-" + String(day()) +
                   " " + String(hour()) + ":" + String(minute()) + ":" + String(second()));
  }

  // Server Routen definieren
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();
}

void loop() {
  server.handleClient(); // kümmert sich um alle HTTP-Clients

  // interne Zeit aktualisieren
  updateInternalTime();

  // prüft den Taster und löscht CSV bei langer Betätigung 
  checkDeleteButton();  

  // Messungen
  if (millis() - lastMeasurement >= MEASURE_INTERVAL) {
    lastMeasurement = millis();
    measureSensor();
  }
  
 
  // NTP-Update alle 60 Sekunden
  if (millis() - lastNTPUpdate >= 60000) {
    lastNTPUpdate = millis();
    if (WiFi.status() == WL_CONNECTED && timeClient.update()) {
      setTime(timeClient.getEpochTime()); // UTC-Zeit korrigieren
      lastMillisUpdate = millis();
      Serial.println("NTP-Zeit aktualisiert: " +
                     String(year()) + "-" + String(month()) + "-" + String(day()) +
                     " " + String(hour()) + ":" + String(minute()) + ":" + String(second()));
    }
  }

  if (millis() - lastPrint > 10000) {
    printCSVPreview();
    lastPrint = millis();
  }

}
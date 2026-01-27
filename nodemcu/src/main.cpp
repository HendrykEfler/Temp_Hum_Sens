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

void logMeasurement() {
  updateInternalTime(); // sicherstellen, dass jetzt aktuelle Zeit verwendet wird

  time_t t = now(); // liefert UTC-Zeit
  char buf[20];
  sprintf(buf, "%04d-%02d-%02d %02d:%02d:%02d",
          year(t), month(t), day(t),
          hour(t), minute(t), second(t));
  String timestamp = String(buf);

  File dataFile = SD.open("data.csv", FILE_WRITE);
  if (dataFile) {
    // Wenn die Datei existiert, ans Ende schreiben
    dataFile.seek(dataFile.size());  // ans Ende springen
    dataFile.print(timestamp);
    dataFile.print(",");
    dataFile.print(temperature);
    dataFile.print(",");
    dataFile.print(humidity);
    dataFile.print(",");
    dataFile.println(pressure);
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

void setup() {
  Serial.begin(115200);
  Wire.begin(D2, D1);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LED_OFF);

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
}
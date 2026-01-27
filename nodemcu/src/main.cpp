#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include "webpage.h"   // HTML auslagern

const char* ssid = "Vodafone-25B4";
const char* password = "qaNPdMPHxC7xmg7x";

ESP8266WebServer server(80);
Adafruit_BME280 bme;

float temperature, humidity, pressure;

unsigned long lastMeasurement = 0;
const unsigned long MEASURE_INTERVAL = 2000; // 2 Sekunden

#define LED_ON  LOW
#define LED_OFF HIGH

void measureSensor() {
  temperature = bme.readTemperature();
  humidity = bme.readHumidity();
  pressure = bme.readPressure() / 100.0F;
  Serial.print("TestAdapter");

  digitalWrite(LED_BUILTIN, LED_ON);
  delay(50);
  digitalWrite(LED_BUILTIN, LED_OFF);
}

void handleRoot() {
  // HTML direkt aus PROGMEM senden, ohne RAM-Overflow
  server.send_P(200, "text/html", MAIN_page);
}

void handleData() {
  // JSON dynamisch generieren
  String json = String("{\"temperature\":") + temperature +
                ",\"humidity\":" + humidity +
                ",\"pressure\":" + pressure + "}";
  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);
  Wire.begin(D2, D1);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LED_OFF);

  if (!bme.begin(0x76)) {
    Serial.println("BME280 nicht gefunden!");
    while (1);
  }

  WiFi.begin(ssid, password);

  Serial.print("Verbinde mit WLAN");
  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
  }

  Serial.println(); // neue Zeile
  Serial.print("NodeMCU verbunden! IP-Adresse: ");
  Serial.println(WiFi.localIP());

  // // <-- Hier den Status ausgeben
  // Serial.print("WiFi Status Code: ");
  // Serial.println(WiFi.status());

  // Server Routen definieren
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();
}

void loop() {
  server.handleClient(); // kümmert sich um alle HTTP-Clients
  if (millis() - lastMeasurement >= MEASURE_INTERVAL) {
    lastMeasurement = millis();
    measureSensor();
  }
}

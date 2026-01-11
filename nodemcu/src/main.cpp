#include <ESP8266WiFi.h>
#include <Wire.h>
#include <Adafruit_BME280.h>
#include "webpage.h"   // HTML auslagern

const char* ssid = "Vodafone-25B4";
const char* password = "qaNPdMPHxC7xmg7x";

WiFiServer server(80);
Adafruit_BME280 bme;

float temperature = 0;
float humidity = 0;
float pressure = 0;

unsigned long lastMeasurement = 0;
const unsigned long MEASURE_INTERVAL = 2000; // 2 Sekunden

#define LED_ON  LOW
#define LED_OFF HIGH

void measureSensor() {
  temperature = bme.readTemperature();
  humidity = bme.readHumidity();
  pressure = bme.readPressure() / 100.0F;

  digitalWrite(LED_BUILTIN, LED_ON);
  delay(50);
  digitalWrite(LED_BUILTIN, LED_OFF);
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

// <-- Hier den Status ausgeben
Serial.print("WiFi Status Code: ");
Serial.println(WiFi.status());

server.begin();
}

void loop() {
  if (millis() - lastMeasurement >= MEASURE_INTERVAL) {
    lastMeasurement = millis();
    measureSensor();
  }
  
  WiFiClient client = server.available();
  if (!client) return;
  String request = client.readStringUntil('\r');
  client.flush();

  if (request.indexOf("GET /data") >= 0) {
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: application/json\n");
    client.print("{\"temperature\":"); client.print(temperature);
    client.print(",\"humidity\":"); client.print(humidity);
    client.print(",\"pressure\":"); client.print(pressure);
    client.print("}");
    client.stop();
    return;
  }

  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html\n");
  client.println(MAIN_page); // HTML aus webpage.h
  client.stop();
}

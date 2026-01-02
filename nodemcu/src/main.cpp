#include <Arduino.h>

void setup() {
  // Initialisierung, z. B. LED-Pin setzen
  pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
  // LED blinken lassen
  digitalWrite(LED_BUILTIN, HIGH);
  delay(500);
  digitalWrite(LED_BUILTIN, LOW);
  delay(500);
}

#include <Arduino.h>
#line 1 "<recovered-sketch-path>"
#define LED_BUILTIN 15  // Common onboard LED pin for ESP32-S2 Mini

#line 3 "<recovered-sketch-path>"
void setup();
#line 7 "<recovered-sketch-path>"
void loop();
#line 3 "<recovered-sketch-path>"
void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
  digitalWrite(LED_BUILTIN, HIGH);  // LED ON
  delay(500);                       // wait 0.5 sec
  digitalWrite(LED_BUILTIN, LOW);   // LED OFF
  delay(500);                       // wait 0.5 sec
}

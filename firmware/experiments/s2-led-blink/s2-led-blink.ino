#define LED_BUILTIN 15  // Common onboard LED pin for ESP32-S2 Mini

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
  digitalWrite(LED_BUILTIN, HIGH);  // LED ON
  delay(500);                       // wait 0.5 sec
  digitalWrite(LED_BUILTIN, LOW);   // LED OFF
  delay(500);                       // wait 0.5 sec
}

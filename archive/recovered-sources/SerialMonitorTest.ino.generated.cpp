#include <Arduino.h>
#line 1 "<recovered-sketch-path>"
const int BUTTON_PIN = 2; // Make sure this matches where your button is wired

#line 3 "<recovered-sketch-path>"
void setup();
#line 12 "<recovered-sketch-path>"
void loop();
#line 3 "<recovered-sketch-path>"
void setup() {
  delay(700);
  Serial.begin(921600);
  pinMode(BUTTON_PIN, INPUT_PULLUP); // Use internal pull-up
  Serial.println("--- Button Test Started ---");
  Serial.println("Press and hold the button connected to GPIO3.");
  Serial.println("Look for 'Button PRESSED' when held.");
}

void loop() {
  int buttonState = digitalRead(BUTTON_PIN);

  if (buttonState == LOW) { // Button is pressed (active low for INPUT_PULLUP)
    Serial.println("Button PRESSED!");
  } else {
    Serial.println("Button RELEASED.");
  }

  delay(100); // Small delay to make output readable, but still responsive
}

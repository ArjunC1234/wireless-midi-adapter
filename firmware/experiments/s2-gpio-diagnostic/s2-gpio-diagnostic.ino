/*
  GPIO_Test.ino - Test ESP32-S2 Pin Functionality
  Tests various GPIO pins to see what survived the short circuit
*/

#define LED_PIN     15  // Built-in LED
#define BUTTON_PIN  0   // Boot button
#define I2C_SDA     4   // Default I2C SDA
#define I2C_SCL     3   // Default I2C SCL

// Test pins
int testPins[] = {1, 2, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18};
int numTestPins = sizeof(testPins) / sizeof(testPins[0]);

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== ESP32-S2 GPIO Test ===");
  
  // Test LED pin
  pinMode(LED_PIN, OUTPUT);
  Serial.println("Testing LED pin...");
  for (int i = 0; i < 5; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(200);
    digitalWrite(LED_PIN, LOW);
    delay(200);
  }
  Serial.printf("LED pin %d: %s\n", LED_PIN, "TESTED");
  
  // Test button pin
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  Serial.printf("Button pin %d reading: %d (should be 1 when not pressed)\n", 
                BUTTON_PIN, digitalRead(BUTTON_PIN));
  
  // Test all GPIO pins as outputs
  Serial.println("\nTesting GPIO pins as outputs...");
  for (int i = 0; i < numTestPins; i++) {
    int pin = testPins[i];
    pinMode(pin, OUTPUT);
    
    // Test HIGH
    digitalWrite(pin, HIGH);
    delay(10);
    pinMode(pin, INPUT);
    int readHigh = digitalRead(pin);
    
    // Test LOW
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
    delay(10);
    pinMode(pin, INPUT);
    int readLow = digitalRead(pin);
    
    Serial.printf("Pin %d: HIGH=%d, LOW=%d ", pin, readHigh, readLow);
    if (readHigh == 1 && readLow == 0) {
      Serial.println("✓ GOOD");
    } else {
      Serial.println("✗ DAMAGED");
    }
    
    delay(100);
  }
  
  Serial.println("\nTest complete!");
  Serial.println("Press button (pin 0) to test input...");
}

void loop() {
  static bool lastButtonState = true;
  static unsigned long lastPrint = 0;
  
  // Test button functionality
  bool buttonState = digitalRead(BUTTON_PIN);
  if (buttonState != lastButtonState) {
    Serial.printf("Button %s\n", buttonState ? "RELEASED" : "PRESSED");
    lastButtonState = buttonState;
  }
  
  // Periodic status
  if (millis() - lastPrint > 5000) {
    Serial.printf("System running... Button state: %d\n", digitalRead(BUTTON_PIN));
    lastPrint = millis();
    
    // Blink LED to show we're alive
    digitalWrite(LED_PIN, HIGH);
    delay(100);
    digitalWrite(LED_PIN, LOW);
  }
  
  delay(50);
}

#include <Arduino.h>
#line 1 "<recovered-sketch-path>"
#include <Wire.h> // Required for I2C communication
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Display Logic: Define the display parameters
#define SCREEN_WIDTH 128    // OLED display width, in pixels
#define SCREEN_HEIGHT 64    // OLED display height, in pixels
#define OLED_RESET -1       // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- Pin Definitions ---
const int LED_PIN = 1;    // Use GPIO2 for wired LED
const int BUTTON_PIN = 2; // Use GPIO3 for wired Button

// --- State Variables ---
bool isPairingMode = false;
bool isPaired = false;
bool currentLedState = LOW;
String deviceName = "None"; // New variable to store the device name
bool displayNeedsUpdate = false; // New variable to control display updates

// --- Button Debouncing and Hold Detection Variables ---
bool lastRawReadingState = HIGH; 
bool stableButtonState = HIGH;   
unsigned long lastDebounceTime = 0; 
const unsigned long DEBOUNCE_DELAY = 50; 

unsigned long buttonPressStartTime = 0; 
const unsigned long BUTTON_HOLD_DURATION = 2000; 
bool buttonWasHeld = false;

// --- Pairing Mode Timeout Variables ---
unsigned long pairingModeStartTime = 0;
const unsigned long PAIRING_MODE_TIMEOUT = 15000; // 15 seconds

// --- LED Blinking Variables ---
unsigned long lastLedToggleTime = 0;
const unsigned long BLINK_INTERVAL = 250;

// --- Serial Debugging Variables (to prevent flooding) ---
bool lastIsPairingModePrint = false;
bool lastIsPairedPrint = false;

// Helper function to print text with word wrapping and centering.
#line 45 "<recovered-sketch-path>"
void printCenteredAndWrappedText(const String& text, int16_t y);
#line 139 "<recovered-sketch-path>"
void updateDisplay();
#line 170 "<recovered-sketch-path>"
void setup();
#line 206 "<recovered-sketch-path>"
void loop();
#line 45 "<recovered-sketch-path>"
void printCenteredAndWrappedText(const String& text, int16_t y) {
  int16_t tx, ty;
  uint16_t tw, th;
  int16_t lineHeight = 8; // Assumes text size 1, 8 pixels tall
  const int16_t HORIZONTAL_PADDING = 4; // Add a small horizontal padding for safety

  // Split the text into an array of words
  String words[20]; // Assuming a maximum of 20 words to fit on the screen
  int wordCount = 0;
  String tempText = text;
  int spaceIndex;
  
  while ((spaceIndex = tempText.indexOf(' ')) != -1 && wordCount < 20) {
    words[wordCount++] = tempText.substring(0, spaceIndex);
    tempText = tempText.substring(spaceIndex + 1);
  }
  if (tempText.length() > 0 && wordCount < 20) {
    words[wordCount++] = tempText;
  }

  String currentLine = "";
  int16_t currentY = y;
  
  Serial.println("------ Word Wrap Test Start ------");

  // Loop through words, build lines, and print as you go
  for (int i = 0; i < wordCount; ++i) {
    String word = words[i];
    String testLine = currentLine.length() > 0 ? currentLine + " " + word : word;
    
    display.getTextBounds(testLine, 0, 0, &tx, &ty, &tw, &th);
    
    Serial.println("------------------------------------");
    Serial.print("Current line: \"");
    Serial.print(currentLine);
    Serial.println("\"");
    Serial.print("Word to add: \"");
    Serial.print(word);
    Serial.println("\"");
    Serial.print("Test line: \"");
    Serial.print(testLine);
    Serial.println("\"");
    Serial.print("Test line width (tw): ");
    Serial.print(tw);
    Serial.println("px");
    Serial.print("SCREEN_WIDTH: ");
    Serial.print(SCREEN_WIDTH);
    Serial.println("px");
    Serial.print("Condition (tw + HORIZONTAL_PADDING >= SCREEN_WIDTH): ");
    Serial.print(tw);
    Serial.print(" + ");
    Serial.print(HORIZONTAL_PADDING);
    Serial.print(" = ");
    Serial.print(tw + HORIZONTAL_PADDING);
    Serial.print(" >= ");
    Serial.print(SCREEN_WIDTH);
    Serial.print("? ");
    Serial.println( (tw + HORIZONTAL_PADDING) >= SCREEN_WIDTH );
    
    if ( (tw + HORIZONTAL_PADDING) >= SCREEN_WIDTH && currentLine.length() > 0) {
      // The word doesn't fit on the current line.
      Serial.println("Condition TRUE. Line break triggered.");
      
      // Add padding and print the current line
      display.getTextBounds(currentLine, 0, 0, &tx, &ty, &tw, &th);
      display.setCursor((SCREEN_WIDTH - tw) / 2, currentY);
      display.println(currentLine);
      
      // Go to a new line
      currentY += lineHeight;
      currentLine = "";
      
      // Re-add the word by decrementing the loop counter
      i--; 
    } else {
      // The word fits, so add it to the current line
      Serial.println("Condition FALSE. Word added to line.");
      currentLine = testLine;
    }
  }
  
  // Print the last line that was built
  if (currentLine.length() > 0) {
    Serial.println("------------------------------------");
    Serial.print("Final line: ");
    Serial.println(currentLine);
    display.getTextBounds(currentLine, 0, 0, &tx, &ty, &tw, &th);
    display.setCursor((SCREEN_WIDTH - tw) / 2, currentY);
    display.println(currentLine);
  }
  Serial.println("------ Word Wrap Test End ------\n");
}

// Display Logic: Function to update the screen based on state
void updateDisplay() {
  Serial.println(">>> Updating display.");
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // --- TOP LINE: High-level Device Status ---
  String highLevelStatus = "Device: " + deviceName;
  printCenteredAndWrappedText(highLevelStatus, 4);

  // --- MIDDLE LINE: Tooltip Information ---
  String tooltip;
  if (isPairingMode) {
    tooltip = "Press button to cancel.";
  } else {
    tooltip = "Hold button for 2s to pair.";
  }
  printCenteredAndWrappedText(tooltip, 28);

  // --- BOTTOM LINE: Detailed Status ---
  String detailedStatus;
  if (isPairingMode) {
    detailedStatus = "Status: PAIRING...";
  } else {
    detailedStatus = ""; // Clear this line when not in pairing mode
  }
  printCenteredAndWrappedText(detailedStatus, 56);
  
  display.display();
}

void setup() {
  Serial.begin(921600);
  Serial.println("Starting Setup...");

  // Display Logic: Explicitly begin I2C communication on your pins
  Wire.begin(8, 9); // Use GPIO8 for SDA and GPIO9 for SCL
  
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C is common for 128x64 OLEDs
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  
  // Clear the display once on setup
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println("Booting...");
  display.display();
  delay(2000); // Wait for a moment

  // Configure pins
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Initial state of LED
  digitalWrite(LED_PIN, LOW);
  currentLedState = LOW;

  Serial.println("Setup complete.");
  Serial.println("Initial State: Not Paired, Not in Pairing Mode. LED is OFF.");
  
  // Initial display update
  updateDisplay();
}

void loop() {
  unsigned long currentMillis = millis();
  
  // --- Read Button State with Debouncing ---
  bool rawReading = digitalRead(BUTTON_PIN);
  if (rawReading != lastRawReadingState) {
    lastDebounceTime = currentMillis;
  }

  if ((currentMillis - lastDebounceTime) > DEBOUNCE_DELAY) {
    if (rawReading != stableButtonState) {
      stableButtonState = rawReading;
      if (stableButtonState == LOW) {
        buttonPressStartTime = currentMillis;
        buttonWasHeld = false;
        Serial.println("Button Pressed. (Debounced New Press Detected)");
      } else {
        if (buttonWasHeld) {
          Serial.println("Button Released after HOLD.");
        } else {
          Serial.println("Button Released (Debounced Quick Press). No Hold Triggered.");
        }
      }
    }
  }

  lastRawReadingState = rawReading;

  // --- Detect Button Hold ---
  bool oldPairingMode = isPairingMode;
  if (stableButtonState == LOW && (currentMillis - buttonPressStartTime) >= BUTTON_HOLD_DURATION && !buttonWasHeld) {
    buttonWasHeld = true;
    Serial.println("Button held down! Toggling Pairing Mode.");
    isPairingMode = !isPairingMode;
    if (isPairingMode) {
      Serial.println("--> Entering Pairing Mode.");
      pairingModeStartTime = currentMillis;
      currentLedState = HIGH;
      digitalWrite(LED_PIN, currentLedState);
      lastLedToggleTime = currentMillis;
    } else {
      Serial.println("--> Exiting Pairing Mode.");
    }
  }
  if (isPairingMode != oldPairingMode) {
    displayNeedsUpdate = true; // Set flag to update display if pairing mode changes
  }

  // --- Pairing Mode Timeout Logic ---
  bool oldPairingModeTimeout = isPairingMode;
  if (isPairingMode && (currentMillis - pairingModeStartTime) >= PAIRING_MODE_TIMEOUT) {
    isPairingMode = false;
    Serial.println("--> Pairing Mode Timed Out. Exiting Pairing Mode.");
  }
  if (isPairingMode != oldPairingModeTimeout) {
     displayNeedsUpdate = true; // Set flag to update display if pairing mode times out
  }

  // --- LED Control Logic (same as before) ---
  if (isPairingMode) {
    if ((currentMillis - lastLedToggleTime) >= BLINK_INTERVAL) {
      currentLedState = !currentLedState;
      digitalWrite(LED_PIN, currentLedState);
      lastLedToggleTime = currentMillis;
    }
  } else {
    if (isPaired) {
      if (currentLedState == LOW) {
        digitalWrite(LED_PIN, HIGH);
        currentLedState = HIGH;
      }
    } else {
      if (currentLedState == HIGH) {
        digitalWrite(LED_PIN, LOW);
        currentLedState = LOW;
      }
    }
  }

  // --- Update device name based on isPaired state and set display flag ---
  bool oldIsPaired = isPaired;
  if (isPaired) {
    deviceName = "MyDevice"; // Example name when paired
  } else {
    deviceName = "None"; // Default name when not paired
  }
  if (isPaired != oldIsPaired) {
    displayNeedsUpdate = true; // Set flag to update display if paired status changes
  }

  // --- Serial Output for State Changes (same as before) ---
  if (isPairingMode != lastIsPairingModePrint) {
      Serial.print("Current State: ");
      if (isPairingMode) {
          Serial.println("IN PAIRING MODE (Blinking LED)");
      } else {
          Serial.print("NOT IN PAIRING MODE. ");
          if (isPaired) {
              Serial.println("Device Paired (Solid LED ON)");
          } else {
              Serial.println("Device Unpaired (LED OFF)");
          }
      }
      lastIsPairingModePrint = isPairingMode;
  }
  
  // --- Conditionally update the display ---
  if (displayNeedsUpdate) {
    updateDisplay();
    displayNeedsUpdate = false; // Reset the flag after updating
  }
}
